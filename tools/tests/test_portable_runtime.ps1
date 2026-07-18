$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script:Failures = [System.Collections.Generic.List[string]]::new()

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        $script:Failures.Add($Message)
    }
}

function Assert-Contains {
    param(
        [string]$Text,
        [string]$Expected,
        [string]$Message
    )

    Assert-True -Condition $Text.Contains($Expected) -Message $Message
}

function Assert-Matches {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    Assert-True -Condition ([regex]::IsMatch($Text, $Pattern)) -Message $Message
}

function Read-RepoText {
    param([string]$RelativePath)

    return Get-Content -Raw -LiteralPath (Join-Path $repoRoot $RelativePath)
}

$assimpRelative = 'Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll'
$assimpDll = Join-Path $repoRoot $assimpRelative
$targetsPath = Join-Path $repoRoot 'Dx11/Directory.Build.targets'

Assert-True -Condition (Test-Path -LiteralPath $assimpDll -PathType Leaf) `
    -Message "Assimp runtime DLL is missing: $assimpRelative"
if (Test-Path -LiteralPath $assimpDll -PathType Leaf) {
    Assert-True -Condition ((Get-Item -LiteralPath $assimpDll).Length -gt 0) `
        -Message 'Assimp runtime DLL is empty.'
}

& git -C $repoRoot check-ignore --quiet -- $assimpRelative
$ignoredExitCode = $LASTEXITCODE
Assert-True -Condition ($ignoredExitCode -ne 0) `
    -Message 'Assimp runtime DLL is still ignored by Git.'

$trackedFiles = @(& git -C $repoRoot ls-files -- $assimpRelative)
Assert-True -Condition ($trackedFiles -contains $assimpRelative) `
    -Message 'Assimp runtime DLL is not tracked by Git.'

$targets = Get-Content -Raw -LiteralPath $targetsPath
Assert-Contains -Text $targets -Expected 'assimp\bin\msvc\assimp-vc143-mt.dll' `
    -Message 'Directory.Build.targets does not name the Assimp runtime DLL.'
Assert-Contains -Text $targets -Expected 'DestinationFolder="$(TargetDir)"' `
    -Message 'Directory.Build.targets does not copy runtime DLLs to TargetDir.'
Assert-Contains -Text $targets -Expected 'DestinationFolder="$(CommonBinDir)"' `
    -Message 'Directory.Build.targets does not copy runtime DLLs to the common bin directory.'

if ($script:Failures.Count -gt 0) {
    Write-Host 'Portable runtime verification failed:'
    foreach ($failure in $script:Failures) {
        Write-Host " - $failure"
    }
    exit 1
}

Write-Host 'Portable runtime verification passed.'
