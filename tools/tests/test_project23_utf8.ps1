$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$relativeFiles = @(
    'Dx11\23_Rigid_Animation\App.cpp',
    'Dx11\23_Rigid_Animation\App.h',
    'Dx11\23_Rigid_Animation\WinMain.cpp',
    'Dx11\23_Rigid_Animation\23_BasicPS.hlsl',
    'Dx11\23_Rigid_Animation\23_BasicVS.hlsl',
    'Dx11\23_Rigid_Animation\23_LightingHelper.hlsli',
    'Dx11\23_Rigid_Animation\23_Shared.fxh',
    'Dx11\23_Rigid_Animation\23_SkyBoxVS.hlsl',
    'Dx11\23_Rigid_Animation\README.md'
)
$bomRequired = @(
    'Dx11\23_Rigid_Animation\App.cpp',
    'Dx11\23_Rigid_Animation\App.h',
    'Dx11\23_Rigid_Animation\WinMain.cpp'
)
$strictUtf8 = [Text.UTF8Encoding]::new($false, $true)
$mojibake = "(?:\uFFFD|Ã|Â|ï§|æ|ë‚|ë©|ì—|ìž|\?��|\?ë|\?ì)"

foreach ($relative in $relativeFiles) {
    $path = Join-Path $repoRoot $relative
    Assert-True (Test-Path -LiteralPath $path -PathType Leaf) "$relative missing"
    $bytes = [IO.File]::ReadAllBytes($path)
    $hasBom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
    if ($relative -in $bomRequired) {
        Assert-True $hasBom "$relative must have a UTF-8 BOM"
    }
    else {
        Assert-True (-not $hasBom) "$relative must not have a UTF-8 BOM"
    }
    try { $text = $strictUtf8.GetString($bytes) }
    catch { throw "$relative is not strict UTF-8: $($_.Exception.Message)" }
    Assert-True ($text -notmatch $mojibake) "$relative still contains mojibake"
}

$readme = [IO.File]::ReadAllText((Join-Path $repoRoot 'Dx11\23_Rigid_Animation\README.md'), $strictUtf8)
$requiredReadmeSentences = @(
    '본의 개수 == 0, 애니메이션 개수 == 0이면 Static Mesh입니다.',
    '본의 개수 > 0, 애니메이션 개수 > 0이면 Skinned Animation입니다.',
    '본의 개수 == 0, 애니메이션 개수 > 0이면 Rigid Animation입니다.',
    '기본 실행 예제는 `../Resource/fbx/Study/BoxHuman.fbx`의 리지드 애니메이션을 자동 재생합니다.'
)
foreach ($sentence in $requiredReadmeSentences) {
    Assert-True $readme.Contains($sentence) "README required sentence missing: $sentence"
}

'project 23 UTF-8 contract tests passed'
