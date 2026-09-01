[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

function Invoke-CleanNativeProcess {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$ArgumentValues,

        [Parameter(Mandatory)]
        [string]$WorkingDirectory
    )

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false

    foreach ($argument in $ArgumentValues) {
        [void]$startInfo.ArgumentList.Add($argument)
    }

    # Windows treats PATH names case-insensitively, but inherited environments
    # can contain both PATH and Path. Normalize them before Process.Start().
    $inheritedEnvironment = [Environment]::GetEnvironmentVariables()
    $pathValue = [Environment]::GetEnvironmentVariable('PATH', 'Process')
    $startInfo.Environment.Clear()
    foreach ($entry in $inheritedEnvironment.GetEnumerator()) {
        if ([string]$entry.Key -ieq 'PATH') {
            continue
        }
        $startInfo.Environment[[string]$entry.Key] = [string]$entry.Value
    }
    $startInfo.Environment['PATH'] = $pathValue

    $process = [Diagnostics.Process]::Start($startInfo)
    $process.WaitForExit()
    return $process.ExitCode
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$projectPath = Join-Path $PSScriptRoot 'native\MaterialTransparencyTests.vcxproj'

$programFilesX86 = [Environment]::GetFolderPath(
    [Environment+SpecialFolder]::ProgramFilesX86)
$vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = $null
if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
    $msbuild = @(& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1)[0]
}
if ([string]::IsNullOrWhiteSpace([string]$msbuild)) {
    $msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($null -ne $msbuildCommand) {
        $msbuild = $msbuildCommand.Source
    }
}
if ([string]::IsNullOrWhiteSpace([string]$msbuild) -or
    -not (Test-Path -LiteralPath $msbuild -PathType Leaf)) {
    throw 'Visual Studio MSBuild is required for the material transparency tests.'
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) (
    'D3D11-MaterialTransparencyTests-' + [guid]::NewGuid().ToString('N'))
$outDir = Join-Path $tempRoot 'out'
$intDir = Join-Path $tempRoot 'obj'
New-Item -ItemType Directory -Force -Path $outDir, $intDir | Out-Null

try {
    $buildInvocation = @{
        FilePath = $msbuild
        WorkingDirectory = $repoRoot
        ArgumentValues = @(
            $projectPath,
            '/t:Build',
            '/p:Configuration=Debug',
            '/p:Platform=x64',
            "/p:RepoRoot=$repoRoot\",
            "/p:OutDir=$outDir\",
            "/p:IntDir=$intDir\",
            '/m:1',
            '/v:minimal'
        )
    }
    $buildExitCode = Invoke-CleanNativeProcess @buildInvocation
    if ($buildExitCode -ne 0) {
        throw "Material transparency tests failed to build (exit $buildExitCode)."
    }

    $testExe = Join-Path $outDir 'MaterialTransparencyTests.exe'
    if (-not (Test-Path -LiteralPath $testExe -PathType Leaf)) {
        throw 'Material transparency test executable was not produced.'
    }

    $testInvocation = @{
        FilePath = $testExe
        WorkingDirectory = $repoRoot
        ArgumentValues = @()
    }
    $testExitCode = Invoke-CleanNativeProcess @testInvocation
    if ($testExitCode -ne 0) {
        throw "Material transparency behavior tests failed (exit $testExitCode)."
    }
}
finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

'Material transparency behavior tests passed.'
