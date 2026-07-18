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

$live2DRelative = 'Dx11/Resource/Live2D/Skeleton_Model'
$live2DDir = Join-Path $repoRoot $live2DRelative
$expectedLive2DFiles = @(
    'Skeleton_Model.model3.json',
    'Skeleton_Model.moc3',
    'Skeleton_Model.cdi3.json',
    'Skeleton_Model.2048/texture_00.png',
    'README.md'
)

foreach ($relativeFile in $expectedLive2DFiles) {
    $assetPath = Join-Path $live2DDir $relativeFile
    Assert-True -Condition (Test-Path -LiteralPath $assetPath -PathType Leaf) `
        -Message "Live2D sample asset is missing: $live2DRelative/$relativeFile"
    if (Test-Path -LiteralPath $assetPath -PathType Leaf) {
        Assert-True -Condition ((Get-Item -LiteralPath $assetPath).Length -gt 0) `
            -Message "Live2D sample asset is empty: $live2DRelative/$relativeFile"
    }
}

$modelJsonPath = Join-Path $live2DDir 'Skeleton_Model.model3.json'
if (Test-Path -LiteralPath $modelJsonPath -PathType Leaf) {
    $modelSettings = Get-Content -Raw -LiteralPath $modelJsonPath | ConvertFrom-Json
    $modelReferences = [System.Collections.Generic.List[string]]::new()
    $modelReferences.Add([string]$modelSettings.FileReferences.Moc)
    foreach ($texture in @($modelSettings.FileReferences.Textures)) {
        $modelReferences.Add([string]$texture)
    }
    if ($modelSettings.FileReferences.DisplayInfo) {
        $modelReferences.Add([string]$modelSettings.FileReferences.DisplayInfo)
    }

    foreach ($modelReference in $modelReferences) {
        Assert-True -Condition (-not [string]::IsNullOrWhiteSpace($modelReference)) `
            -Message 'Live2D model3.json contains an empty file reference.'
        if (-not [string]::IsNullOrWhiteSpace($modelReference)) {
            $referencedPath = Join-Path $live2DDir $modelReference
            Assert-True -Condition (Test-Path -LiteralPath $referencedPath -PathType Leaf) `
                -Message "Live2D model3.json reference is missing: $modelReference"
        }
    }
}

$provenancePath = Join-Path $live2DDir 'README.md'
if (Test-Path -LiteralPath $provenancePath -PathType Leaf) {
    $provenance = Get-Content -Raw -LiteralPath $provenancePath
    Assert-Contains -Text $provenance `
        -Expected 'https://github.com/BluePengcho/Open_Source_Hand_Tracking_Live2D_Model' `
        -Message 'Live2D provenance README does not name the upstream repository.'
    Assert-Contains -Text $provenance `
        -Expected '994c4719f081a3f219b62abbeb4a4b43543a48b8' `
        -Message 'Live2D provenance README does not pin the upstream commit.'
}

$live2DHeader = Read-RepoText -RelativePath 'Dx11/11_Live2D/App.h'
$live2DSource = Read-RepoText -RelativePath 'Dx11/11_Live2D/App.cpp'

Assert-Contains -Text $live2DHeader `
    -Expected 'bool LoadLive2DModel(const std::wstring& model3Path);' `
    -Message 'App.h does not declare the shared Live2D load helper.'
Assert-Contains -Text $live2DSource `
    -Expected 'L"..\\Resource\\Live2D\\Skeleton_Model\\Skeleton_Model.model3.json"' `
    -Message 'App.cpp does not name the bundled Live2D startup model.'
Assert-Matches -Text $live2DSource `
    -Pattern 'bool\s+App::LoadLive2DModel\s*\(const\s+std::wstring&\s+model3Path\)' `
    -Message 'App.cpp does not define App::LoadLive2DModel.'
Assert-Contains -Text $live2DSource `
    -Expected 'LoadLive2DModel(kDefaultLive2DModelPath);' `
    -Message 'OnInitialize does not load the bundled Live2D model.'
Assert-Contains -Text $live2DSource `
    -Expected 'LoadLive2DModel(file);' `
    -Message 'The model file picker does not use the shared load helper.'

if ($script:Failures.Count -gt 0) {
    Write-Host 'Portable runtime verification failed:'
    foreach ($failure in $script:Failures) {
        Write-Host " - $failure"
    }
    exit 1
}

Write-Host 'Portable runtime verification passed.'
