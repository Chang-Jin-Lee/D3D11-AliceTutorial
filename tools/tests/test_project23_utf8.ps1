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
$strictUtf8 = [Text.UTF8Encoding]::new($false, $true)
$mojibake = "(?:\uFFFD|Ã|Â|ï§|æ|ë‚|ë©|ì—|ìž|\?��|\?ë|\?ì)"

foreach ($relative in $relativeFiles) {
    $path = Join-Path $repoRoot $relative
    Assert-True (Test-Path -LiteralPath $path -PathType Leaf) "$relative missing"
    $bytes = [IO.File]::ReadAllBytes($path)
    try { $text = $strictUtf8.GetString($bytes) }
    catch { throw "$relative is not strict UTF-8: $($_.Exception.Message)" }
    Assert-True ($text -notmatch $mojibake) "$relative still contains mojibake"
}

$readme = [IO.File]::ReadAllText((Join-Path $repoRoot 'Dx11\23_Rigid_Animation\README.md'), $strictUtf8)
Assert-True ($readme -match '리지드 애니메이션|Rigid Animation') 'README rigid-animation explanation missing'
Assert-True ($readme -match '본의 개수') 'README Korean lesson text missing'

'project 23 UTF-8 contract tests passed'
