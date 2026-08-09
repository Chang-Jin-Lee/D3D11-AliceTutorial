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
$sourcePath = Join-Path $repoRoot 'Dx11\23_Rigid_Animation\App.cpp'
$source = [IO.File]::ReadAllText($sourcePath, [Text.Encoding]::UTF8)
$anchor = $source.IndexOf('bool App::OnInitialize()', [StringComparison]::Ordinal)
Assert-True ($anchor -ge 0) 'project 23 OnInitialize missing'
$body = Get-BracedBlock $source $anchor 'project 23 OnInitialize'

$load = 'LoadModelFromFile(L"..\\Resource\\fbx\\Study\\BoxHuman.fbx")'
Assert-True ([regex]::Matches($body, [regex]::Escape($load)).Count -eq 1) `
    'project 23 must load BoxHuman.fbx exactly once'
Assert-True ($body -notmatch 'SampleModel\.glb') 'project 23 must not load SampleModel.glb at startup'
Assert-True ($body -match 'model\.pos\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\)') 'origin missing'
Assert-True ($body -match 'model\.rotDeg\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\)') 'zero rotation missing'
Assert-True ($body -match 'model\.scale\s*=\s*XMFLOAT3\(\s*1\.0f\s*,\s*1\.0f\s*,\s*1\.0f\s*\)') 'unit scale missing'
Assert-True ($body -match 'model\.autoRotate\s*=\s*false') 'whole-model auto rotation must be disabled'
Assert-True ($body -match 'GetCurrentAnimationType\(\)\s*==\s*FbxManager::AnimationType::Rigid') 'rigid classification guard missing'
Assert-True ($body -match 'model\.fbx\.HasAnimations\(\)') 'embedded-animation guard missing'
Assert-True ($body -match 'model\.uiAnimPlaying\s*=\s*true') 'UI playback state missing'
Assert-True ($body -match 'model\.fbx\.SetAnimationPlaying\(\s*true\s*\)') 'FbxManager playback state missing'
Assert-True ($body -match 'm_Camera\.SetPosition\(\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*-8\.0f\s*\)\s*\)') 'project camera position missing'
Assert-True ($body -match 'm_Camera\.SetRotation\(\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\)\s*\)') 'project camera rotation missing'

$loadCondition = 'if\s*\(\s*' + [regex]::Escape($load) + '\s*&&\s*!m_->m_Models\.empty\(\)\s*\)'
Assert-True ($body -match $loadCondition) 'successful BoxHuman load guard missing'
$loadAnchor = $body.IndexOf(('if (' + $load), [StringComparison]::Ordinal)
Assert-True ($loadAnchor -ge 0) 'BoxHuman load block missing'
$loadBlock = Get-BracedBlock $body $loadAnchor 'BoxHuman load block'

Assert-True ($loadBlock -match 'model\.pos\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\)') 'origin must be inside successful load block'
Assert-True ($loadBlock -match 'model\.rotDeg\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\)') 'zero rotation must be inside successful load block'
Assert-True ($loadBlock -match 'model\.scale\s*=\s*XMFLOAT3\(\s*1\.0f\s*,\s*1\.0f\s*,\s*1\.0f\s*\)') 'unit scale must be inside successful load block'
Assert-True ($loadBlock -match 'model\.autoRotate\s*=\s*false') 'auto rotation state must be inside successful load block'
Assert-True ($loadBlock -match 'm_Camera\.SetPosition\(\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*-8\.0f\s*\)\s*\)') 'camera position must be inside successful load block'
Assert-True ($loadBlock -match 'm_Camera\.SetRotation\(\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\)\s*\)') 'camera rotation must be inside successful load block'

$autoplayAnchor = $loadBlock.IndexOf('if (model.source == ModelSource::FBX', [StringComparison]::Ordinal)
Assert-True ($autoplayAnchor -ge 0) 'FBX autoplay block missing'
$autoplayBlock = Get-BracedBlock $loadBlock $autoplayAnchor 'FBX autoplay block'
Assert-True ($autoplayBlock -match 'model\.source\s*==\s*ModelSource::FBX') 'FBX source guard missing from autoplay block'
Assert-True ($autoplayBlock -match 'GetCurrentAnimationType\(\)\s*==\s*FbxManager::AnimationType::Rigid') 'rigid classification guard must be inside autoplay block'
Assert-True ($autoplayBlock -match 'model\.fbx\.HasAnimations\(\)') 'embedded-animation guard must be inside autoplay block'
Assert-True ([regex]::Matches($body, 'model\.uiAnimPlaying\s*=\s*true').Count -eq 1) 'UI playback state must be set exactly once'
Assert-True ([regex]::Matches($body, 'model\.fbx\.SetAnimationPlaying\(\s*true\s*\)').Count -eq 1) 'FbxManager playback state must be set exactly once'
Assert-True ($autoplayBlock -match 'model\.uiAnimPlaying\s*=\s*true') 'UI playback state must be inside autoplay block'
Assert-True ($autoplayBlock -match 'model\.fbx\.SetAnimationPlaying\(\s*true\s*\)') 'FbxManager playback state must be inside autoplay block'
Assert-True ($body -notmatch 'ReadmeCapture::IsEnabled\(\)') 'project 23 startup must not substitute the README capture scene'

'project 23 rigid startup contract tests passed'
