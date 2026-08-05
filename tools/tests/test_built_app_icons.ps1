param(
    [Parameter(Mandatory = $true)][string]$BinRoot,
    [Parameter(Mandatory = $true)][datetime]$NotOlderThan,
    [string[]]$ProjectNames = @(),
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$AdditionalProjectNames = @()
)

$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class NativeIconProbe {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool FreeLibrary(IntPtr module);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadImageW(IntPtr instance, IntPtr name, uint type, int width, int height, uint flags);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool DestroyIcon(IntPtr icon);
}
'@

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$resolvedBinRoot = if ([IO.Path]::IsPathRooted($BinRoot)) {
    (Resolve-Path -LiteralPath $BinRoot).Path
}
else {
    (Resolve-Path -LiteralPath (Join-Path $repoRoot $BinRoot)).Path
}

$hasExplicitSelection = $PSBoundParameters.ContainsKey('ProjectNames') -or $PSBoundParameters.ContainsKey('AdditionalProjectNames')
$ProjectNames = @($ProjectNames) + @($AdditionalProjectNames)

$solutionText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\TutorialApp.sln')
$solutionProjectNames = @(
    [regex]::Matches($solutionText, '(?m)^Project\("[^"]+"\) = "([^"]+)", "([^"]+\.vcxproj)"') |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -ne 'Common' }
)
Assert-True ($solutionProjectNames.Count -eq 37) "solution must contain exactly 37 application projects, got $($solutionProjectNames.Count)"
Assert-True (@($solutionProjectNames | Sort-Object -Unique).Count -eq 37) 'solution application projects must be unique'
foreach ($solutionProjectName in $solutionProjectNames) {
    Assert-True ($solutionProjectName -match '^[A-Za-z0-9_]+$') "invalid solution project name: $solutionProjectName"
}

if ($hasExplicitSelection) {
    Assert-True ($ProjectNames.Count -gt 0) 'explicit application project selection must not be empty'
}
else {
    $ProjectNames = $solutionProjectNames
}

Assert-True ($ProjectNames.Count -gt 0) 'no application projects selected'
Assert-True (@($ProjectNames | Sort-Object -Unique).Count -eq $ProjectNames.Count) 'selected application projects must be unique'
foreach ($projectName in $ProjectNames) {
    Assert-True ($projectName -match '^[A-Za-z0-9_]+$') "invalid project name: $projectName"
    Assert-True ($solutionProjectNames -contains $projectName) "selected project is not an application in TutorialApp.sln: $projectName"
}

# Visual Studio's canonical x64 Debug output is distinct from the post-build
# runtime copy in Dx11\bin. Probe only this deterministic build product so both
# fresh copies can coexist without making executable selection ambiguous.
$canonicalOutputRoot = Join-Path $resolvedBinRoot 'x64\Debug'
Assert-True (Test-Path -LiteralPath $canonicalOutputRoot -PathType Container) "canonical output directory missing: $canonicalOutputRoot"

foreach ($projectName in $ProjectNames) {
    $executable = Join-Path $canonicalOutputRoot "$projectName.exe"
    Assert-True (Test-Path -LiteralPath $executable -PathType Leaf) "canonical executable missing: $executable"
    $executableInfo = Get-Item -LiteralPath $executable
    Assert-True ($executableInfo.LastWriteTime -ge $NotOlderThan) "canonical executable is stale: $executable"

    $module = [NativeIconProbe]::LoadLibraryExW($executableInfo.FullName, [IntPtr]::Zero, 0x00000002)
    Assert-True ($module -ne [IntPtr]::Zero) "LoadLibraryExW failed for $projectName with $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    try {
        foreach ($size in @(32, 16)) {
            $icon = [NativeIconProbe]::LoadImageW($module, [IntPtr]::new(101), 1, $size, $size, 0)
            Assert-True ($icon -ne [IntPtr]::Zero) "resource 101 ${size}px missing from $projectName with $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
            $null = [NativeIconProbe]::DestroyIcon($icon)
        }
    }
    finally {
        $null = [NativeIconProbe]::FreeLibrary($module)
    }
}

"built app icon resources verified: $($ProjectNames.Count) executables"
