[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'native_test_common.ps1')

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$projectPath = Join-Path $PSScriptRoot 'native\CoverageSceneTests.vcxproj'
$testExitCode = Invoke-CoverageNativeTest -ProjectPath $projectPath `
    -TestName 'CoverageSceneTests' -RepoRoot $repoRoot

if ($testExitCode -eq 2) {
    Write-Warning 'Coverage scene behavior tests skipped by the native test executable.'
    exit 2
}
if ($testExitCode -ne 0) {
    [Console]::Error.WriteLine("Coverage scene behavior tests failed (exit $testExitCode).")
    exit $testExitCode
}

'Coverage scene behavior tests passed.'
