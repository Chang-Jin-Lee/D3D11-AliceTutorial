$ErrorActionPreference = 'Stop'

function Invoke-CoverageNativeProcess {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][AllowEmptyCollection()][string[]]$ArgumentValues,
        [Parameter(Mandatory)][string]$WorkingDirectory
    )

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
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
    $standardOutput = $process.StandardOutput.ReadToEndAsync()
    $standardError = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    [Console]::Out.Write($standardOutput.GetAwaiter().GetResult())
    [Console]::Error.Write($standardError.GetAwaiter().GetResult())
    return $process.ExitCode
}

function Find-CoverageVs2022MsBuild {
    $buildToolsMsbuild = Join-Path ([Environment]::GetFolderPath(
            [Environment+SpecialFolder]::ProgramFilesX86)) `
        'Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe'
    if (Test-Path -LiteralPath $buildToolsMsbuild -PathType Leaf) {
        return $buildToolsMsbuild
    }

    $vswhere = Join-Path ([Environment]::GetFolderPath(
            [Environment+SpecialFolder]::ProgramFilesX86)) `
        'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $candidate = @(& $vswhere -latest -version '[17.0,18.0)' -products '*' `
                -requires Microsoft.Component.MSBuild `
                -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1)[0]
        if (-not [string]::IsNullOrWhiteSpace([string]$candidate) -and
            (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return $candidate
        }
    }

    throw 'Visual Studio 2022 MSBuild is required for native coverage tests.'
}

function Invoke-CoverageNativeTest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ProjectPath,
        [Parameter(Mandatory)][string]$TestName,
        [Parameter(Mandatory)][string]$RepoRoot
    )

    $resolvedProject = (Resolve-Path -LiteralPath $ProjectPath).Path
    $resolvedRepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
    $msbuild = Find-CoverageVs2022MsBuild

    $tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    $tempPrefix = "D3D11-$TestName-"
    $tempRoot = [IO.Path]::GetFullPath(
        (Join-Path $tempBase ($tempPrefix + [guid]::NewGuid().ToString('N'))))
    $tempLeaf = Split-Path -Leaf $tempRoot
    if (-not $tempRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase) -or
        -not $tempLeaf.StartsWith($tempPrefix, [StringComparison]::Ordinal)) {
        throw "Refusing unsafe temporary test directory: $tempRoot"
    }

    $outDir = Join-Path $tempRoot 'out'
    $intDir = Join-Path $tempRoot 'obj'
    New-Item -ItemType Directory -Force -Path $outDir, $intDir | Out-Null

    try {
        $buildExitCode = Invoke-CoverageNativeProcess -FilePath $msbuild `
            -WorkingDirectory $resolvedRepoRoot -ArgumentValues @(
                $resolvedProject,
                '/t:Build',
                '/p:Configuration=Debug',
                '/p:Platform=x64',
                "/p:RepoRoot=$resolvedRepoRoot\",
                "/p:OutDir=$outDir\",
                "/p:IntDir=$intDir\",
                '/m:1',
                '/nr:false',
                '/p:BuildInParallel=false',
                '/v:minimal'
            )
        if ($buildExitCode -ne 0) { return $buildExitCode }

        $testExe = Join-Path $outDir "$TestName.exe"
        if (-not (Test-Path -LiteralPath $testExe -PathType Leaf)) {
            Write-Error "Native test executable was not produced: $testExe"
            return 1
        }

        return Invoke-CoverageNativeProcess -FilePath $testExe `
            -WorkingDirectory $resolvedRepoRoot -ArgumentValues @($resolvedRepoRoot)
    }
    finally {
        $safeTempRoot = $tempRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase) -and
            (Split-Path -Leaf $tempRoot).StartsWith($tempPrefix, [StringComparison]::Ordinal)
        if ($safeTempRoot) {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}
