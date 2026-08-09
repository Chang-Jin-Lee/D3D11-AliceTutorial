$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-BracedBlock([string]$Text, [int]$Anchor, [string]$Label) {
    $open = $Text.IndexOf('{', $Anchor)
    Assert-True ($open -ge 0) "$Label opening brace missing"
    $depth = 0
    for ($i = $open; $i -lt $Text.Length; ++$i) {
        if ($Text[$i] -eq '{') { ++$depth }
        elseif ($Text[$i] -eq '}') {
            --$depth
            if ($depth -eq 0) { return $Text.Substring($Anchor, $i - $Anchor + 1) }
        }
    }
    throw "$Label closing brace missing"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourcePath = Join-Path $repoRoot 'Dx11\Common\FbxManager.cpp'
$source = [IO.File]::ReadAllText($sourcePath)
$anchor = $source.IndexOf('void FbxManager::UpdateAnimation(', [StringComparison]::Ordinal)
Assert-True ($anchor -ge 0) 'FbxManager::UpdateAnimation missing'
$body = Get-BracedBlock $source $anchor 'FbxManager::UpdateAnimation'

Assert-True ($body -match 'if\s*\(\s*m_CurrentType\s*!=\s*AnimationType::Skinned\s*\)\s*return\s*;') `
    'static FBX models must return before skinning palette evaluation'
Assert-True ($body -notmatch 'if\s*\(\s*!m_->HasAnimations\s*\|\|\s*m_->CurrentClip\s*<\s*0\s*\)\s*return\s*;') `
    'skinned models without clips must not return before bind-pose evaluation'
Assert-True ($body -match 'const\s+aiAnimation\s*\*\s*anim\s*=.*m_->HasAnimations.*nullptr') `
    'animation channel selection must allow a null clip'
Assert-True ($body -match 'EvaluateGlobalMatrices\s*\(\s*scene\s*,\s*channelOf\s*,\s*global\s*\)') `
    'bind-pose hierarchy evaluation missing'
Assert-True ($body -match 'BuildBonePalette\s*\(\s*global\s*,\s*palette\s*\)') `
    'bind-pose palette construction missing'
Assert-True ($body -match 'UploadBonePalette\s*\(\s*ctx\s*,\s*palette\s*\)') `
    'bind-pose palette upload missing'

'bind pose without animation contract tests passed'
