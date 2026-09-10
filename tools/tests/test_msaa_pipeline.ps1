[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'native_test_common.ps1')

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$projectPath = Join-Path $PSScriptRoot 'native\MsaaPipelineTests.vcxproj'
$testExitCode = Invoke-CoverageNativeTest -ProjectPath $projectPath `
    -TestName 'MsaaPipelineTests' -RepoRoot $repoRoot

if ($testExitCode -eq 2) {
    Write-Warning 'MSAA pipeline WARP pixel tests skipped because 4x support is unavailable.'
    exit 2
}
if ($testExitCode -ne 0) {
    [Console]::Error.WriteLine("MSAA pipeline WARP pixel tests failed (exit $testExitCode).")
    exit $testExitCode
}

'MSAA pipeline WARP pixel tests passed.'
