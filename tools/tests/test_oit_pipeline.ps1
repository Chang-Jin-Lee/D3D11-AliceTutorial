[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

function Invoke-CleanNativeProcess {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][AllowEmptyCollection()][string[]]$ArgumentValues,
        [Parameter(Mandatory)][string]$WorkingDirectory
    )

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    foreach ($argument in $ArgumentValues) {
        [void]$startInfo.ArgumentList.Add($argument)
    }

    $inheritedEnvironment = [Environment]::GetEnvironmentVariables()
    $pathValue = [Environment]::GetEnvironmentVariable('PATH', 'Process')
    $startInfo.Environment.Clear()
    foreach ($entry in $inheritedEnvironment.GetEnumerator()) {
        if ([string]$entry.Key -ieq 'PATH') { continue }
        $startInfo.Environment[[string]$entry.Key] = [string]$entry.Value
    }
    $startInfo.Environment['PATH'] = $pathValue

    $process = [Diagnostics.Process]::Start($startInfo)
    $process.WaitForExit()
    return $process.ExitCode
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$projectPath = Join-Path $PSScriptRoot 'native\OitPipelineTests.vcxproj'
$buildToolsMsbuild = Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)) `
    'Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe'
$msbuild = if (Test-Path -LiteralPath $buildToolsMsbuild -PathType Leaf) { $buildToolsMsbuild } else { $null }
if ([string]::IsNullOrWhiteSpace([string]$msbuild)) {
    $vswhere = Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)) `
        'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $msbuild = @(& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild `
            -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1)[0]
    }
}
if ([string]::IsNullOrWhiteSpace([string]$msbuild)) {
    $msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($null -ne $msbuildCommand) { $msbuild = $msbuildCommand.Source }
}
if ([string]::IsNullOrWhiteSpace([string]$msbuild) -or
    -not (Test-Path -LiteralPath $msbuild -PathType Leaf)) {
    throw 'Visual Studio 2022 MSBuild is required for the OIT pipeline tests.'
}

$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$tempRoot = Join-Path $tempBase ('D3D11-OitPipelineTests-' + [guid]::NewGuid().ToString('N'))
$tempRoot = [IO.Path]::GetFullPath($tempRoot)
$tempLeaf = Split-Path -Leaf $tempRoot
if (-not $tempRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase) -or
    -not $tempLeaf.StartsWith('D3D11-OitPipelineTests-', [StringComparison]::Ordinal)) {
    throw "Refusing unsafe temporary test directory: $tempRoot"
}
$outDir = Join-Path $tempRoot 'out'
$intDir = Join-Path $tempRoot 'obj'
New-Item -ItemType Directory -Force -Path $outDir, $intDir | Out-Null

try {
    $buildExitCode = Invoke-CleanNativeProcess -FilePath $msbuild -WorkingDirectory $repoRoot -ArgumentValues @(
        $projectPath,
        '/t:Build',
        '/p:Configuration=Debug',
        '/p:Platform=x64',
        "/p:RepoRoot=$repoRoot\",
        "/p:OutDir=$outDir\",
        "/p:IntDir=$intDir\",
        '/m:1',
        '/nr:false',
        '/p:BuildInParallel=false',
        '/v:minimal'
    )
    if ($buildExitCode -ne 0) { throw "OIT pipeline tests failed to build (exit $buildExitCode)." }

    $testExe = Join-Path $outDir 'OitPipelineTests.exe'
    if (-not (Test-Path -LiteralPath $testExe -PathType Leaf)) {
        throw 'OIT pipeline test executable was not produced.'
    }
    $testExitCode = Invoke-CleanNativeProcess -FilePath $testExe -WorkingDirectory $repoRoot `
        -ArgumentValues @($repoRoot)
    if ($testExitCode -ne 0) { throw "OIT pipeline behavior tests failed (exit $testExitCode)." }
}
finally {
    if ($tempRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $tempRoot).StartsWith('D3D11-OitPipelineTests-', [StringComparison]::Ordinal)) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

'OIT pipeline WARP behavior tests passed.'
