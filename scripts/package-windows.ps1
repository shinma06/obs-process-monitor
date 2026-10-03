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
    # A reused build tree can contain arbitrary generated headers that neither
    # --fresh nor a clean target removes. Give each package a new empty tree.
    $artifactDir = Join-Path $ProjectRoot ('out/package-' + [guid]::NewGuid().ToString('N'))
    $buildDir = Join-Path $artifactDir 'build'
    New-Item -ItemType Directory -Path $buildDir | Out-Null
    cmake --preset windows-x64 -B $buildDir '-DOBS_PLUGIN_PACKAGE_BUILD:BOOL=ON'
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed; no package was created.' }
    $buildInfo = Get-Content -LiteralPath (Join-Path $buildDir 'build-info.json') -Raw | ConvertFrom-Json
    if ($buildInfo.source_sha -ne $sourceSha) { throw 'Configured source revision does not match HEAD.' }
    cmake --build $buildDir --config RelWithDebInfo --parallel
    if ($LASTEXITCODE -ne 0) { throw 'CMake build failed; no package was created.' }
    ctest --test-dir $buildDir -C RelWithDebInfo --output-on-failure --no-tests=error
    if ($LASTEXITCODE -ne 0) { throw 'CTest failed; no package was created.' }
    $spec = Get-Content -LiteralPath 'buildspec.json' -Raw | ConvertFrom-Json
    $stage = Join-Path $artifactDir 'stage'
    New-Item -ItemType Directory -Path $stage | Out-Null
    cmake --install $buildDir --config RelWithDebInfo --prefix $stage
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
    $sources = Join-Path $artifactDir ('obs-process-monitor-' + $sourceSha.Substring(0,12) + '-source.zip')
    # Keep a failed native archive command outside the publishable artifact glob.
    $sourceArchive = Join-Path $buildDir 'source.zip'
    git archive --format=zip --output=$sourceArchive HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Source archive failed.' }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $package
    Move-Item -LiteralPath $sourceArchive -Destination $sources
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
