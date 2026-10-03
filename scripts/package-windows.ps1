[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $ProjectRoot
try {
    $sourceSha = (git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read source revision.' }
    git diff --quiet HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Commit tracked changes before packaging an identified build.' }
    # Do not use --exclude-standard: ignored headers/resources can still be
    # compiler or install inputs. Only known non-source output locations are safe.
    $untracked = @(git ls-files --others -- . `
        ':(top,exclude)build_x64/**' ':(top,exclude).deps/**' `
        ':(top,exclude)out/**' ':(top,exclude).harness-local/**' `
        ':(top,exclude).vs/**' ':(top,exclude)cmake/.CMakeBuildNumber' `
        ':(top,exclude,glob)scripts/__pycache__/*.pyc' `
        ':(top,exclude,glob)tests/__pycache__/*.pyc')
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect untracked package inputs.' }
    if ($untracked.Count -ne 0) {
        # Paths can contain private names; keep them out of CI/public diagnostics.
        throw 'Commit or remove untracked files outside documented build output locations before packaging.'
    }
    $buildInfo = Get-Content -LiteralPath 'build_x64/build-info.json' -Raw | ConvertFrom-Json
    if ($buildInfo.source_sha -ne $sourceSha) { throw 'Reconfigure after changing HEAD before packaging.' }
    # Discard cache settings left by an untracked CMake module from an earlier
    # configure, even if that input was removed before this packaging attempt.
    cmake --preset windows-x64 --fresh
    if ($LASTEXITCODE -ne 0) { throw 'Fresh CMake configure failed; no package was created.' }
    $buildInfo = Get-Content -LiteralPath 'build_x64/build-info.json' -Raw | ConvertFrom-Json
    if ($buildInfo.source_sha -ne $sourceSha) { throw 'Fresh configure source revision does not match HEAD.' }
    # A prior build may have compiled a now-removed untracked header. Recompile
    # from the verified source tree rather than trusting incremental objects.
    cmake --build --preset windows-x64 --parallel --clean-first
    if ($LASTEXITCODE -ne 0) { throw 'CMake build failed; no package was created.' }
    $spec = Get-Content -LiteralPath 'buildspec.json' -Raw | ConvertFrom-Json
    $artifactDir = Join-Path $ProjectRoot ('out/package-' + [guid]::NewGuid().ToString('N'))
    $stage = Join-Path $artifactDir 'stage'
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    cmake --install build_x64 --config RelWithDebInfo --prefix $stage
    if ($LASTEXITCODE -ne 0) { throw 'CMake install failed.' }
    $dll = Join-Path $stage 'obs-process-monitor/bin/64bit/obs-process-monitor.dll'
    if (!(Test-Path -LiteralPath $dll -PathType Leaf)) { throw 'The package DLL is missing.' }
    $dllHash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
    $manifest = [ordered]@{
        source_sha = $sourceSha
        source_url = ('https://github.com/shinma06/obs-process-monitor/commit/' + $sourceSha)
        dll_path = 'obs-process-monitor/bin/64bit/obs-process-monitor.dll'
        dll_sha256 = $dllHash
        configuration = 'RelWithDebInfo'
        toolchain = $buildInfo
        dependencies = $spec.dependencies
        runner_image = $env:ImageVersion
        workflow_run = $env:GITHUB_RUN_ID
    }
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $stage 'build-manifest.json') -Encoding utf8
    $package = Join-Path $artifactDir ('obs-process-monitor-' + $sourceSha.Substring(0,12) + '-windows-x64.zip')
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $package
    $sources = Join-Path $artifactDir ('obs-process-monitor-' + $sourceSha.Substring(0,12) + '-source.zip')
    git archive --format=zip --output=$sources HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Source archive failed.' }
    $hashes = foreach ($file in @($package, $sources)) {
        $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
        $hash + '  ' + (Split-Path -Leaf $file)
    }
    $hashes | Set-Content -LiteralPath (Join-Path $artifactDir 'SHA256SUMS.txt') -Encoding ascii
    Copy-Item -LiteralPath (Join-Path $stage 'build-manifest.json') -Destination $artifactDir
    Write-Output ('Source SHA: ' + $sourceSha)
    Write-Output ('DLL SHA256: ' + $dllHash)
    Write-Output ('Artifact directory: ' + $artifactDir)
    if ($env:GITHUB_STEP_SUMMARY) {
        @('Source: `' + $sourceSha + '`', 'DLL SHA-256: `' + $dllHash + '`') | Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY
    }
} finally {
    Pop-Location
}
