[CmdletBinding()]
param(
    [string]$PackageScript = (Join-Path $PSScriptRoot 'package-windows.ps1'),
    [switch]$ExpectLegacyLeaks
)
$ErrorActionPreference = 'Stop'
$packageText = Get-Content -LiteralPath $PackageScript -Raw
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('obs-package-inputs-' + [guid]::NewGuid().ToString('N'))
$oldGlobalConfig = $env:GIT_CONFIG_GLOBAL
$oldNoSystem = $env:GIT_CONFIG_NOSYSTEM
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
function Write-FixtureFile([string]$Path, [string]$Text = 'fixture') {
    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    [IO.File]::WriteAllText($Path, $Text)
}
function Invoke-FixtureGit {
    & git @args | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Fixture Git command failed.' }
}
# The real package entry point runs up to the build boundary. No compiler,
# downloads or synthetic package are needed to prove rejected inputs never build.
function cmake {
    if ($args -contains '--preset' -and $args -notcontains '--build') {
        if ($args -notcontains '--fresh') { throw 'Fixture expected a fresh configure.' }
        $script:freshConfigured = $true
        $global:LASTEXITCODE = 0
        return
    }
    if ($args -notcontains '--build') { throw 'Unexpected fixture CMake call.' }
    if (-not $ExpectLegacyLeaks -and -not $script:freshConfigured) { throw 'Fixture expected configure before build.' }
    if (-not $ExpectLegacyLeaks -and $args -notcontains '--clean-first') {
        throw 'Fixture expected a clean rebuild after checking inputs.'
    }
    throw 'FIXTURE_BUILD_REACHED'
}
$cases = @(
    @{ Name = 'clean'; Kind = 'accept' },
    @{ Name = 'generated-outputs'; Kind = 'accept'; Files = @('build_x64/plugin-macros.generated.h', '.deps/QtCore', 'out/package-old/stage/data/a.ini', '.harness-local/qa/private.json', '.vs/local.json', 'cmake/.CMakeBuildNumber', 'scripts/__pycache__/check.cpython-311.pyc', 'tests/__pycache__/test_harness.cpython-311.pyc') },
    @{ Name = 'root-shadow-header'; Kind = 'untracked'; Files = @('plugin-macros.generated.h') },
    @{ Name = 'nested-shadow-header'; Kind = 'untracked'; Files = @('src/metrics/windows.h') },
    @{ Name = 'untracked-data'; Kind = 'untracked'; Files = @('data/locale/extra.ini') },
    @{ Name = 'repository-ignored-data'; Kind = 'untracked'; Files = @('data/extra.dll') },
    @{ Name = 'local-exclude-data'; Kind = 'untracked'; Files = @('data/locale/local.ini'); LocalExclude = 'data/locale/local.ini' },
    @{ Name = 'global-exclude-header'; Kind = 'untracked'; Files = @('plugin-macros.generated.h'); GlobalExclude = 'plugin-macros.generated.h' },
    @{ Name = 'global-exclude-data'; Kind = 'untracked'; Files = @('data/locale/global.ini'); GlobalExclude = 'data/locale/global.ini' },
    @{ Name = 'nested-output-name-is-input'; Kind = 'untracked'; Files = @('data/build_x64/extra.ini', 'data/__pycache__/extra.pyc') },
    @{ Name = 'untracked-cmake-module'; Kind = 'untracked'; Files = @('cmake/common/FindLocal.cmake') },
    @{ Name = 'local-user-preset'; Kind = 'untracked'; Files = @('CMakeUserPresets.json') },
    @{ Name = 'private-config-rejected-without-path'; Kind = 'untracked'; Files = @('.env') },
    @{ Name = 'unstaged-tracked'; Kind = 'tracked'; Files = @('plugin-main.cpp') },
    @{ Name = 'staged-tracked'; Kind = 'tracked'; Files = @('data/locale/en-US.ini'); Stage = $true }
)
try {
    $globalConfig = Join-Path $fixtureRoot 'gitconfig'
    $globalExclude = Join-Path $fixtureRoot 'global-exclude'
    $env:GIT_CONFIG_GLOBAL = $globalConfig
    $env:GIT_CONFIG_NOSYSTEM = '1'
    Write-FixtureFile $globalConfig ("[core]`n    excludesFile = " + $globalExclude.Replace('\', '/') + "`n")
    foreach ($case in $cases) {
        Write-FixtureFile $globalExclude ''
        $repo = Join-Path $fixtureRoot $case.Name
        New-Item -ItemType Directory -Path $repo | Out-Null
        Push-Location $repo
        try {
            Invoke-FixtureGit init --quiet
            Invoke-FixtureGit config user.name 'Package fixture'
            Invoke-FixtureGit config user.email 'fixture@example.invalid'
            Invoke-FixtureGit config core.autocrlf false
            Write-FixtureFile (Join-Path $repo 'scripts/package-windows.ps1') $packageText
            Write-FixtureFile (Join-Path $repo 'plugin-main.cpp') '#include "plugin-macros.generated.h"'
            Write-FixtureFile (Join-Path $repo 'data/locale/en-US.ini') 'Name=Fixture'
            Write-FixtureFile (Join-Path $repo '.gitignore') "build_x64/`n.deps/`nout/`n.harness-local/`n.vs/`n*.dll`n*.pyc`n.env`nCMakeUserPresets.json`ncmake/.CMakeBuildNumber`n"
            Invoke-FixtureGit add .
            Invoke-FixtureGit commit --quiet -m fixture
            $sha = (& git rev-parse HEAD).Trim()
            if ($LASTEXITCODE -ne 0) { throw 'Fixture HEAD unavailable.' }
            Write-FixtureFile (Join-Path $repo 'build_x64/build-info.json') ('{"source_sha":"' + $sha + '"}')
            foreach ($file in $case.Files) { Write-FixtureFile (Join-Path $repo $file) 'changed fixture' }
            if ($case.LocalExclude) { Write-FixtureFile (Join-Path $repo '.git/info/exclude') $case.LocalExclude }
            if ($case.GlobalExclude) { Write-FixtureFile $globalExclude $case.GlobalExclude }
            if ($case.Stage) { Invoke-FixtureGit add -- $case.Files }
            $script:freshConfigured = $false
            $failure = ''
            try { & (Join-Path $repo 'scripts/package-windows.ps1') | Out-Null }
            catch { $failure = $_.Exception.Message }
            $expected = if ($case.Kind -eq 'tracked') { 'Commit tracked changes before packaging an identified build.' }
                        elseif ($case.Kind -eq 'untracked' -and -not $ExpectLegacyLeaks) { 'Commit or remove untracked files outside documented build output locations before packaging.' }
                        else { 'FIXTURE_BUILD_REACHED' }
            if ($failure -ne $expected) { throw ('Package input regression failed: ' + $case.Name + '; boundary: ' + $failure) }
            Write-Output ('PASS ' + $case.Name)
        } finally { Pop-Location }
    }
    Write-Output ("Package input tests passed: $($cases.Count); legacy leak reproduction: $ExpectLegacyLeaks")
} finally {
    $env:GIT_CONFIG_GLOBAL = $oldGlobalConfig
    $env:GIT_CONFIG_NOSYSTEM = $oldNoSystem
    # Delete only the unique fixture directory created under the resolved temp root.
    $resolvedFixture = [IO.Path]::GetFullPath($fixtureRoot)
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolvedFixture.StartsWith($resolvedTemp, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture cleanup containment failed.' }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
