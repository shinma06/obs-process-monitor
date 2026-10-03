[CmdletBinding()]
param(
    [string]$PackageScript = '',
    [switch]$ExpectLegacyLeaks
)
$ErrorActionPreference = 'Stop'
if (-not $PackageScript) { $PackageScript = Join-Path $PSScriptRoot 'package-windows.ps1' }
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
# Exercise the real package entry point with native-tool boundaries replaced.
# Rejected source inputs must never configure, and a failed build/test must never
# install or create public artifacts. The real CI separately builds and runs CTest.
function cmake {
    if ($ExpectLegacyLeaks) { throw 'FIXTURE_BUILD_REACHED' }
    if ($args -contains '--preset') {
        $fixtureState.Events.Add('configure')
        if ($args[$args.IndexOf('--preset') + 1] -ne 'windows-x64' -or $args -notcontains '-B') {
            throw 'Fixture expected the pinned preset and explicit new build tree.'
        }
        $fixtureState.BuildDir = $args[$args.IndexOf('-B') + 1]
        if ($fixtureState.BuildDir -notmatch '[\\/]out[\\/]package-[0-9a-f]{32}[\\/]build$' -or
            @(Get-ChildItem -LiteralPath $fixtureState.BuildDir -Force).Count -ne 0) {
            throw 'Fixture expected a new empty package build tree.'
        }
        if ($fixtureState.FailureStage -eq 'configure') { $global:LASTEXITCODE = 1; return }
        $configuredSha = if ($fixtureState.FailureStage -eq 'identity') { 'wrong-revision' } else { $sha }
        Write-FixtureFile (Join-Path $fixtureState.BuildDir 'build-info.json') ('{"source_sha":"' + $configuredSha + '"}')
        $global:LASTEXITCODE = 0
        return
    }
    if ($args -contains '--build') {
        $fixtureState.Events.Add('build')
        if ($args[$args.IndexOf('--build') + 1] -ne $fixtureState.BuildDir -or $args -contains '--preset' -or
            $args[$args.IndexOf('--config') + 1] -ne 'RelWithDebInfo') { throw 'Fixture build tree/config mismatch.' }
        $global:LASTEXITCODE = if ($fixtureState.FailureStage -eq 'build') { 1 } else { 0 }
        return
    }
    if ($args -contains '--install') {
        $fixtureState.Events.Add('install')
        if ($args[$args.IndexOf('--install') + 1] -ne $fixtureState.BuildDir -or -not $fixtureState.TestsPassed -or
            $args[$args.IndexOf('--config') + 1] -ne 'RelWithDebInfo') { throw 'Fixture installed an untested build.' }
        if ($fixtureState.FailureStage -eq 'install') { $global:LASTEXITCODE = 1; return }
        throw 'FIXTURE_INSTALL_REACHED'
    }
    throw 'Unexpected fixture CMake call.'
}
function ctest {
    $fixtureState.Events.Add('test')
    if ($args[$args.IndexOf('--test-dir') + 1] -ne $fixtureState.BuildDir -or
        $args[$args.IndexOf('-C') + 1] -ne 'RelWithDebInfo' -or $args -notcontains '--no-tests=error' -or
        ($fixtureState.Events -join ',') -ne 'configure,build,test') { throw 'Fixture CTest tree/config/order mismatch.' }
    $fixtureState.TestsPassed = $fixtureState.FailureStage -ne 'test'
    $global:LASTEXITCODE = if ($fixtureState.TestsPassed) { 0 } else { 1 }
}
$cases = @(
    @{ Name = 'clean'; Kind = 'accept' },
    @{ Name = 'generated-outputs'; Kind = 'accept'; Files = @('build_x64/plugin-macros.generated.h', 'build_x64/obs-module.h', 'build_x64/CMakeCache.txt', 'out/package-previous/build/obs-module.h', '.deps/QtCore', 'out/package-old/stage/data/a.ini', '.harness-local/qa/private.json', '.vs/local.json', 'cmake/.CMakeBuildNumber', 'scripts/__pycache__/check.cpython-311.pyc', 'tests/__pycache__/test_harness.cpython-311.pyc') },
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
if (-not $ExpectLegacyLeaks) {
    $cases += @(
        @{ Name = 'configure-failure'; Kind = 'flow'; FailAt = 'configure'; Expected = 'CMake configure failed; no package was created.'; Events = 'configure' },
        @{ Name = 'configured-identity-mismatch'; Kind = 'flow'; FailAt = 'identity'; Expected = 'Configured source revision does not match HEAD.'; Events = 'configure' },
        @{ Name = 'build-failure'; Kind = 'flow'; FailAt = 'build'; Expected = 'CMake build failed; no package was created.'; Events = 'configure,build' },
        @{ Name = 'ctest-failure'; Kind = 'flow'; FailAt = 'test'; Expected = 'CTest failed; no package was created.'; Events = 'configure,build,test' },
        @{ Name = 'install-failure'; Kind = 'flow'; FailAt = 'install'; Expected = 'CMake install failed.'; Events = 'configure,build,test,install' }
    )
}
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
            Write-FixtureFile (Join-Path $repo 'buildspec.json') '{"dependencies":{}}'
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
            $fixtureState = @{ Events = [System.Collections.Generic.List[string]]::new(); BuildDir = ''; TestsPassed = $false; FailureStage = $case.FailAt }
            $failure = ''
            try { & (Join-Path $repo 'scripts/package-windows.ps1') | Out-Null }
            catch { $failure = $_.Exception.Message }
            $expected = if ($case.Kind -eq 'tracked') { 'Commit tracked changes before packaging an identified build.' }
                        elseif ($case.Kind -eq 'untracked' -and -not $ExpectLegacyLeaks) { 'Commit or remove untracked files outside documented build output locations before packaging.' }
                        elseif ($case.Expected) { $case.Expected }
                        elseif ($ExpectLegacyLeaks) { 'FIXTURE_BUILD_REACHED' }
                        else { 'FIXTURE_INSTALL_REACHED' }
            if ($failure -ne $expected) { throw ('Package input regression failed: ' + $case.Name + '; boundary: ' + $failure) }
            if (-not $ExpectLegacyLeaks) {
                $expectedEvents = if ($case.Events) { $case.Events } elseif ($case.Kind -eq 'accept') { 'configure,build,test,install' } else { '' }
                if (($fixtureState.Events -join ',') -ne $expectedEvents) { throw ('Unexpected package phase after ' + $case.Name) }
                if (@(Get-ChildItem -Path 'out/package-*/*.zip', 'out/package-*/*.json' -ErrorAction SilentlyContinue).Count -ne 0) {
                    throw ('Public artifact created before successful install: ' + $case.Name)
                }
            }
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
