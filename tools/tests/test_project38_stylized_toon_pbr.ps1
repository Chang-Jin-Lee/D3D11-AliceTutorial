$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-CppFunctionBody([string]$Text, [string]$SignaturePattern) {
    $signature = [regex]::Match($Text, $SignaturePattern)
    Assert-True $signature.Success "C++ function missing: $SignaturePattern"

    $openBrace = $Text.IndexOf('{', $signature.Index + $signature.Length)
    Assert-True ($openBrace -ge 0) "C++ function body missing: $SignaturePattern"

    $depth = 0
    for ($index = $openBrace; $index -lt $Text.Length; ++$index) {
        if ($Text[$index] -eq '{') { ++$depth }
        elseif ($Text[$index] -eq '}') {
            --$depth
            if ($depth -eq 0) {
                return $Text.Substring($openBrace, $index - $openBrace + 1)
            }
        }
    }

    throw "Unterminated C++ function body: $SignaturePattern"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$projectName = '38_StylizedToonPBR'
$projectGuid = '{4B6A5522-0C57-41E0-A222-6AA813BBCE5C}'
$projectDirectory = Join-Path $repoRoot "Dx11\$projectName"
$projectPath = Join-Path $projectDirectory "$projectName.vcxproj"
$filtersPath = "$projectPath.filters"
$readmePath = Join-Path $projectDirectory 'README.md'
$appHeaderPath = Join-Path $projectDirectory 'App.h'
$appSourcePath = Join-Path $projectDirectory 'App.cpp'
$profilerHeaderPath = Join-Path $projectDirectory 'GpuProfiler.h'
$profilerSourcePath = Join-Path $projectDirectory 'GpuProfiler.cpp'
$rootReadmePath = Join-Path $repoRoot 'README.md'
$project37ReadmePath = Join-Path $repoRoot 'Dx11\37_Blueprint\README.md'
$shaderFiles = @(
    '38_Shared.fxh',
    '38_CharacterVS.hlsl',
    '38_CharacterPS.hlsl',
    '38_FullscreenVS.hlsl',
    '38_OutlinePS.hlsl',
    '38_ToneMapPS.hlsl'
)

$solutionText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\TutorialApp.sln')
$solutionProjectNames = @(
    [regex]::Matches($solutionText, '(?m)^Project\("[^"]+"\) = "([^"]+)", "([^"]+\.vcxproj)"') |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -ne 'Common' }
)
Assert-True ($solutionProjectNames.Count -eq 38) 'solution must contain 38 applications'
Assert-True (@($solutionProjectNames | Sort-Object -Unique).Count -eq 38) 'solution application projects must be unique'
Assert-True ($solutionText -match [regex]::Escape($projectName)) 'Project 38 missing from solution'
Assert-True ($solutionText -match ('(?im)^Project\("[^"]+"\) = "{0}", "{0}\\{0}\.vcxproj", "{1}"$' -f [regex]::Escape($projectName), [regex]::Escape($projectGuid))) 'Project 38 solution registration must use the fixed project GUID and path'

foreach ($mapping in @(
    'Debug\|x64\.ActiveCfg = Debug\|x64',
    'Debug\|x64\.Build\.0 = Debug\|x64',
    'Debug\|x86\.ActiveCfg = Debug\|Win32',
    'Debug\|x86\.Build\.0 = Debug\|Win32',
    'Release\|x64\.ActiveCfg = Release\|x64',
    'Release\|x64\.Build\.0 = Release\|x64',
    'Release\|x86\.ActiveCfg = Release\|Win32',
    'Release\|x86\.Build\.0 = Release\|Win32'
)) {
    Assert-True ($solutionText -match ('(?im)^\s*{0}\.{1}\s*$' -f [regex]::Escape($projectGuid), $mapping)) "Project 38 solution mapping missing: $mapping"
}

$manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\readme_media_manifest.json') | ConvertFrom-Json
Assert-True ([int]$manifest.expectedProjectCount -eq 38) 'manifest count must be 38'
Assert-True (@($manifest.projects).Count -eq 38) 'manifest must contain 38 projects'
$project38Entries = @($manifest.projects | Where-Object number -eq '38')
Assert-True ($project38Entries.Count -eq 1) 'manifest Project 38 entry missing'
$project38 = $project38Entries[0]
Assert-True ($project38.directory -ceq $projectName) 'Project 38 manifest directory mismatch'
Assert-True ($project38.name -ceq 'Stylized Toon PBR') 'Project 38 manifest name mismatch'
Assert-True ($project38.exe -ceq "$projectName.exe") 'Project 38 manifest executable mismatch'
Assert-True ($project38.image -ceq '38-StylizedToonPBR.png') 'Project 38 PNG path mismatch'
Assert-True ($project38.gif -ceq '38-StylizedToonPBR.gif') 'Project 38 GIF path mismatch'
Assert-True ($project38.infoImage -ceq 'info/38-StylizedToonPBR-info.png') 'Project 38 info-image path mismatch'
Assert-True ([int]$project38.delayMs -eq 3000) 'Project 38 capture delay mismatch'
Assert-True ([bool]$project38.readmeCaptureMode) 'Project 38 must opt into README capture mode'
Assert-True ($project38.title -ceq 'Stylized Toon PBR') 'Project 38 manifest title mismatch'
Assert-True ($project38.summary -ceq '재질별 Hybrid Toon-PBR와 외곽선 비용을 비교합니다.') 'Project 38 manifest summary mismatch'
Assert-True (($project38.tags -join ',') -ceq 'Toon PBR,Outline,GPU Profiling,Character') 'Project 38 manifest tags mismatch'
Assert-True ($project38.gifPhase -ceq 'runtime') 'Project 38 GIF phase mismatch'
Assert-True ($null -eq $project38.gifPresentationPan) 'Project 38 must not change the presentation-pan allowlist'

$targetsText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\Directory.Build.targets')
Assert-True ($targetsText -match ';38_StylizedToonPBR;') 'Project 38 app icon allowlist entry missing'

foreach ($path in @(
    $projectPath,
    $filtersPath,
    (Join-Path $projectDirectory 'WinMain.cpp'),
    $appHeaderPath,
    $appSourcePath,
    $profilerHeaderPath,
    $profilerSourcePath,
    $readmePath
) + @(
    $shaderFiles | ForEach-Object { Join-Path $projectDirectory $_ }
)) {
    Assert-True (Test-Path -LiteralPath $path -PathType Leaf) "Project 38 required file missing: $path"
}

[xml]$projectXml = Get-Content -Raw -LiteralPath $projectPath
$namespace = [Xml.XmlNamespaceManager]::new($projectXml.NameTable)
$namespace.AddNamespace('msb', 'http://schemas.microsoft.com/developer/msbuild/2003')
Assert-True ($projectXml.SelectSingleNode('//msb:ProjectGuid', $namespace).InnerText.ToUpperInvariant() -ceq $projectGuid) 'Project 38 vcxproj GUID mismatch'

$configurations = @($projectXml.SelectNodes('//msb:ProjectConfiguration', $namespace) | ForEach-Object { $_.Include })
foreach ($configuration in @('Debug|Win32', 'Release|Win32', 'Debug|x64', 'Release|x64')) {
    Assert-True ($configurations -contains $configuration) "Project 38 configuration missing: $configuration"
}

$debugX64 = $projectXml.SelectSingleNode("//msb:ItemDefinitionGroup[@Condition=`"'`$(Configuration)|`$(Platform)'=='Debug|x64'`"]", $namespace)
Assert-True ($null -ne $debugX64) 'Project 38 Debug x64 item definitions missing'
Assert-True ($debugX64.ClCompile.LanguageStandard -ceq 'stdcpp20') 'Project 38 Debug x64 must use C++20'

$projectReference = $projectXml.SelectSingleNode('//msb:ProjectReference[@Include="..\Common\Common.vcxproj"]', $namespace)
Assert-True ($null -ne $projectReference) 'Project 38 must reference Common.vcxproj'
Assert-True ($projectReference.Project.ToUpperInvariant() -ceq '{05774CF5-5EB5-455B-8ADF-707FB11F2F9F}') 'Project 38 Common.vcxproj GUID mismatch'

$compileFiles = @($projectXml.SelectNodes('//msb:ClCompile', $namespace) | ForEach-Object { $_.Include })
$includeFiles = @($projectXml.SelectNodes('//msb:ClInclude', $namespace) | ForEach-Object { $_.Include })
Assert-True ((@($compileFiles | Sort-Object) -join ',') -ceq (@('App.cpp', 'GpuProfiler.cpp', 'WinMain.cpp') -join ',')) 'Project 38 vcxproj must declare the Task 3 C++ sources'
Assert-True ((@($includeFiles | Sort-Object) -join ',') -ceq (@('App.h', 'GpuProfiler.h') -join ',')) 'Project 38 vcxproj must declare the Task 3 headers'
$declaredShaderFiles = @($projectXml.SelectNodes('//msb:FxCompile | //msb:CopyFileToFolders | //msb:None[contains(@Include, ".hlsl") or contains(@Include, ".hlsli") or contains(@Include, ".fxh")]', $namespace) | ForEach-Object { $_.Include })
Assert-True ((@($declaredShaderFiles | Sort-Object) -join ',') -ceq (@($shaderFiles | Sort-Object) -join ',')) 'break: Project 38 must publish exactly the six planned HLSL/FXH runtime files'

$projectText = Get-Content -Raw -LiteralPath $projectPath
Assert-True ($projectText -match '\$\(SolutionDir\)Resource\\\*') 'Project 38 must copy the public Resource directory'
Assert-True ($projectText -match '\$\(OutDir\)\\\.\.\\Resource\\') 'Project 38 runtime Resource destination mismatch'

[xml]$filtersXml = Get-Content -Raw -LiteralPath $filtersPath
$filtersNamespace = [Xml.XmlNamespaceManager]::new($filtersXml.NameTable)
$filtersNamespace.AddNamespace('msb', 'http://schemas.microsoft.com/developer/msbuild/2003')
$filteredCompiles = @($filtersXml.SelectNodes('//msb:ClCompile', $filtersNamespace) | ForEach-Object { $_.Include })
$filteredIncludes = @($filtersXml.SelectNodes('//msb:ClInclude', $filtersNamespace) | ForEach-Object { $_.Include })
Assert-True ((@($filteredCompiles | Sort-Object) -join ',') -ceq (@('App.cpp', 'GpuProfiler.cpp', 'WinMain.cpp') -join ',')) 'Project 38 filters must declare the Task 3 C++ sources'
Assert-True ((@($filteredIncludes | Sort-Object) -join ',') -ceq (@('App.h', 'GpuProfiler.h') -join ',')) 'Project 38 filters must declare the Task 3 headers'
$filteredShaderFiles = @($filtersXml.SelectNodes('//msb:FxCompile | //msb:CopyFileToFolders | //msb:None[contains(@Include, ".hlsl") or contains(@Include, ".hlsli") or contains(@Include, ".fxh")]', $filtersNamespace) | ForEach-Object { $_.Include })
Assert-True ((@($filteredShaderFiles | Sort-Object) -join ',') -ceq (@($shaderFiles | Sort-Object) -join ',')) 'break: Project 38 filters must register exactly the six planned HLSL/FXH files'

$profilerHeaderText = Get-Content -Raw -LiteralPath $profilerHeaderPath
$profilerSourceText = Get-Content -Raw -LiteralPath $profilerSourcePath

Assert-True ($profilerHeaderText -match '(?s)enum\s+class\s+GpuPass\s*:\s*uint8_t\s*\{\s*Shadow\s*,\s*Character\s*,\s*Outline\s*,\s*ToneMap\s*,\s*Count\s*\}') 'GpuPass must expose Shadow, Character, Outline, and ToneMap in order'
Assert-True ($profilerHeaderText -match '(?s)struct\s+GpuTimings\s*\{[^}]*bool\s+valid[^}]*double\s+totalMs[^}]*std::array\s*<\s*double\s*,\s*4\s*>\s+passMs') 'GpuTimings public result contract missing'
foreach ($method in @(
    'bool\s+Initialize\s*\(\s*ID3D11Device\s*\*\s*device\s*\)',
    'void\s+BeginFrame\s*\(\s*ID3D11DeviceContext\s*\*\s*context\s*\)',
    'void\s+BeginPass\s*\(\s*ID3D11DeviceContext\s*\*\s*context\s*,\s*GpuPass\s+pass\s*\)',
    'void\s+EndPass\s*\(\s*ID3D11DeviceContext\s*\*\s*context\s*,\s*GpuPass\s+pass\s*\)',
    'void\s+EndFrame\s*\(\s*ID3D11DeviceContext\s*\*\s*context\s*\)',
    'void\s+Resolve\s*\(\s*ID3D11DeviceContext\s*\*\s*context\s*\)',
    'const\s+GpuTimings\s*&\s*Latest\s*\(\s*\)\s*const'
)) {
    Assert-True ($profilerHeaderText -match $method) "GpuProfiler public method missing: $method"
}

$slotCount = [regex]::Match($profilerHeaderText, 'kQuerySlotCount\s*=\s*(\d+)')
Assert-True $slotCount.Success 'GpuProfiler query-slot count missing'
Assert-True ([int]$slotCount.Groups[1].Value -eq 4) 'GpuProfiler must use exactly four query slots'
Assert-True ($profilerHeaderText -match '(?s)struct\s+PassQueries\s*\{[^}]*beginTimestamp[^}]*endTimestamp') 'GpuProfiler must keep paired begin/end timestamp queries'
Assert-True ($profilerHeaderText -match 'std::array\s*<\s*PassQueries\s*,\s*kPassCount\s*>') 'Every query slot must own timestamp pairs for every GPU pass'

Assert-True ($profilerSourceText -match '\bD3D11_QUERY_TIMESTAMP_DISJOINT\b') 'GpuProfiler disjoint query creation missing'
Assert-True ($profilerSourceText -match '\bD3D11_QUERY_TIMESTAMP\b') 'GpuProfiler timestamp query creation missing'
Assert-True ($profilerSourceText -notmatch '\bwhile\s*\(') 'GpuProfiler must not poll GetData in a while loop'

$beginFrameBody = Get-CppFunctionBody $profilerSourceText 'void\s+GpuProfiler::BeginFrame\s*\([^)]*\)'
$beginPassBody = Get-CppFunctionBody $profilerSourceText 'void\s+GpuProfiler::BeginPass\s*\([^)]*\)'
$endPassBody = Get-CppFunctionBody $profilerSourceText 'void\s+GpuProfiler::EndPass\s*\([^)]*\)'
$endFrameBody = Get-CppFunctionBody $profilerSourceText 'void\s+GpuProfiler::EndFrame\s*\([^)]*\)'
$resolveBody = Get-CppFunctionBody $profilerSourceText 'void\s+GpuProfiler::Resolve\s*\([^)]*\)'
Assert-True ($beginFrameBody -match 'Begin\s*\([^;]*disjoint') 'BeginFrame must begin the current slot disjoint query'
Assert-True ($beginPassBody -match 'End\s*\([^;]*beginTimestamp') 'BeginPass must emit the selected pass begin timestamp'
Assert-True ($endPassBody -match 'End\s*\([^;]*endTimestamp') 'EndPass must emit the selected pass end timestamp'
Assert-True ($endFrameBody -match 'End\s*\([^;]*disjoint') 'EndFrame must end the matching slot disjoint query'
Assert-True ($endFrameBody -match 'submitted\s*=\s*true') 'EndFrame must mark the completed query slot submitted'

$resolveDelay = [regex]::Match($profilerHeaderText, 'kResolveDelay\s*=\s*(\d+)')
Assert-True $resolveDelay.Success 'GpuProfiler delayed resolve constant missing'
Assert-True (([int]$resolveDelay.Groups[1].Value -gt 0) -and ([int]$resolveDelay.Groups[1].Value -lt [int]$slotCount.Groups[1].Value)) 'GpuProfiler must resolve an older ring slot'
Assert-True ($resolveBody -match '\bkResolveDelay\b') 'Resolve must select its slot using the delayed resolve constant'
Assert-True ($resolveBody -match 'if\s*\([^)]*!\s*slot\.submitted[^)]*\)\s*\{\s*return\s*;') 'Resolve must ignore a slot that has not been submitted'

$getDataCallCount = [regex]::Matches($resolveBody, '\bGetData\s*\(').Count
$nonFlushingGetDataCount = [regex]::Matches($resolveBody, '(?s)\bGetData\s*\((?:(?!;).)*\bD3D11_ASYNC_GETDATA_DONOTFLUSH\b(?:(?!;).)*;').Count
$immediateReturnCount = [regex]::Matches($resolveBody, '(?s)if\s*\(\s*context->GetData\s*\((?:(?!;).)*\bD3D11_ASYNC_GETDATA_DONOTFLUSH\b(?:(?!;).)*!=\s*S_OK\s*\)\s*\{\s*return\s*;').Count
Assert-True ($getDataCallCount -ge 3) 'Resolve must read disjoint, begin, and end query results'
Assert-True ($nonFlushingGetDataCount -eq $getDataCallCount) 'Every GetData call must use D3D11_ASYNC_GETDATA_DONOTFLUSH'
Assert-True ($immediateReturnCount -eq $getDataCallCount) 'Resolve must immediately return when any query result is not ready'
Assert-True ($resolveBody -match 'disjointData\.Disjoint\s*\|\|\s*disjointData\.Frequency\s*==\s*0') 'Resolve must discard disjoint or zero-frequency samples'
Assert-True ($resolveBody -match '1000\.0\s*/\s*static_cast\s*<\s*double\s*>\s*\(\s*disjointData\.Frequency\s*\)') 'Resolve must convert matching-frequency ticks to milliseconds'
Assert-True ($resolveBody -match 'GpuTimings\s+resolved') 'Resolve must assemble timings locally before publishing'
Assert-True ([regex]::Matches($resolveBody, 'm_latest\s*=').Count -eq 1) 'Resolve must publish Latest only once after a complete sample'
Assert-True ($resolveBody.LastIndexOf('m_latest =') -gt $resolveBody.LastIndexOf('GetData')) 'Resolve must preserve Latest until every query result is ready'

$appHeaderText = Get-Content -Raw -LiteralPath $appHeaderPath
$appSourceText = Get-Content -Raw -LiteralPath $appSourcePath
$sharedShaderText = Get-Content -Raw -LiteralPath (Join-Path $projectDirectory '38_Shared.fxh')
$characterVertexShaderText = Get-Content -Raw -LiteralPath (Join-Path $projectDirectory '38_CharacterVS.hlsl')
$characterPixelShaderText = Get-Content -Raw -LiteralPath (Join-Path $projectDirectory '38_CharacterPS.hlsl')
$fullscreenVertexShaderText = Get-Content -Raw -LiteralPath (Join-Path $projectDirectory '38_FullscreenVS.hlsl')
$outlinePixelShaderText = Get-Content -Raw -LiteralPath (Join-Path $projectDirectory '38_OutlinePS.hlsl')
$toneMapPixelShaderText = Get-Content -Raw -LiteralPath (Join-Path $projectDirectory '38_ToneMapPS.hlsl')

# Review-fix contracts are collected so one RED run proves every reported semantic gap.
$reviewContractFailures = [Collections.Generic.List[string]]::new()
function Assert-ReviewContract([bool]$Condition, [string]$Message) {
    if (-not $Condition) { $script:reviewContractFailures.Add($Message) }
}

$createWindowResourcesBody = Get-CppFunctionBody $appSourceText 'bool\s+App::CreateWindowSizeResources\s*\([^)]*\)'
$renderCharacterPassBody = Get-CppFunctionBody $appSourceText 'void\s+App::RenderCharacterPass\s*\([^)]*\)'
$renderToneMapPassBody = Get-CppFunctionBody $appSourceText 'void\s+App::RenderToneMapPass\s*\([^)]*\)'

$linearDepthOutlineContract =
    ($sharedShaderText -match '\bdepthReconstructionParameters\b') -and
    ($outlinePixelShaderText -match 'float\s+ReconstructViewDepth\s*\(') -and
    ($outlinePixelShaderText -match '(?s)nearPlane\s*\*\s*farPlane\s*/\s*max\s*\(\s*farPlane\s*-\s*deviceDepth\s*\*\s*\(\s*farPlane\s*-\s*nearPlane') -and
    ($outlinePixelShaderText -match '\brelativeDepthDifference\b') -and
    ($outlinePixelShaderText -match 'centerHasGeometry\s*!=\s*sampleHasGeometry') -and
    ($outlinePixelShaderText -notmatch 'abs\s*\(\s*centerDepth\s*-\s*sampleDepth\s*\)\s*\*\s*45')
Assert-ReviewContract $linearDepthOutlineContract 'review: outline depth edges must reconstruct view depth, preserve silhouette transitions, and avoid raw nonlinear-depth scaling'

$alphaAwareShadowContract =
    ($characterPixelShaderText -match 'void\s+PSShadow\s*\(') -and
    ($characterPixelShaderText -match '(?s)PSShadow[^\{]*\{.*?baseColorTexture\.Sample.*?clip\s*\([^;]*alphaCutoff') -and
    ($appSourceText -match 'CompileShader\s*\(\s*L"38_CharacterPS\.hlsl"\s*,\s*"PSShadow"\s*,\s*"ps_5_0"') -and
    ($appSourceText -match '\bm_shadowPixelShader\b') -and
    ($appSourceText -match '\bAI_MATKEY_GLTF_ALPHAMODE\b') -and
    ($appSourceText -match '\bAI_MATKEY_GLTF_ALPHACUTOFF\b') -and
    ($appSourceText -match '\bAI_MATKEY_TWOSIDED\b') -and
    ($appSourceText -match '(?s)shadowOnly.*?PSSetShaderResources\s*\(\s*0\s*,\s*1[^;]*baseColor') -and
    ($appSourceText -match '\bm_shadowDoubleSidedRasterizerState\b') -and
    ($appSourceText -match '\bm_characterDoubleSidedRasterizerState\b')
Assert-ReviewContract $alphaAwareShadowContract 'review: shadow silhouettes must use bound base alpha/cutoff and the same per-material double-sided semantics as the visible pass'

$outputTransferContract =
    ($toneMapPixelShaderText -match 'float3\s+LinearToSrgb\s*\(') -and
    ($toneMapPixelShaderText -match '\b0\.0031308f?\b') -and
    ($toneMapPixelShaderText -match '\b12\.92f?\b') -and
    ($toneMapPixelShaderText -match '(?s)pow\s*\([^;]*1\.0f\s*/\s*2\.4f') -and
    ($toneMapPixelShaderText -match 'return\s+float4\s*\(\s*LinearToSrgb\s*\(')
Assert-ReviewContract $outputTransferContract 'review: UNORM backbuffer output must apply an explicit linear-to-sRGB transfer after tone mapping and outline compositing'

$optionalNormalContract =
    ($createWindowResourcesBody -match 'const\s+bool\s+normalProfileAvailable\s*=\s*createColorTarget') -and
    ($createWindowResourcesBody -match '(?s)if\s*\(\s*!\s*createColorTarget\s*\([^;]*m_hdrTexture[^;]*\)\s*\)\s*return\s+false') -and
    ($createWindowResourcesBody -match '(?s)if\s*\(\s*normalProfileAvailable\s*\).*?m_outlineTexture') -and
    ($renderCharacterPassBody -match '\brenderTargetCount\b') -and
    ($renderCharacterPassBody -match 'if\s*\(\s*m_normalProfileRenderTargetView\s*\)') -and
    ($renderToneMapPassBody -match 'm_outlineShaderResourceView\s*\?') -and
    ($renderToneMapPassBody -match 'm_normalProfileShaderResourceView\s*\?')
Assert-ReviewContract $optionalNormalContract 'review: normal/profile allocation must be optional, reduce MRT count safely, disable outlines, and bind valid tone-map fallbacks'

$shadowSoftnessHudContract = $appSourceText -match 'ImGui::SliderFloat\s*\(\s*"Shadow softness"\s*,\s*&m_shadowSoftness'
Assert-ReviewContract $shadowSoftnessHudContract 'review: compact HUD must expose shadow softness'

if ($reviewContractFailures.Count -gt 0) {
    throw ("Review fix contracts failed:`n - " + ($reviewContractFailures -join "`n - "))
}

# Break caught: another model path silently replaces or supplements the approved public Alice asset.
$modelPathMatches = [regex]::Matches($appSourceText, '(?i)\.\.[\\/]Resource[\\/]fbx[\\/][^"\r\n]+')
Assert-True ($modelPathMatches.Count -eq 1) 'break: Project 38 must reference exactly one runtime FBX/GLB model path'
Assert-True ($modelPathMatches[0].Value -ceq '..\Resource\fbx\Public\MyAlice\Player\SampleModel.glb') 'break: Project 38 must load only the approved public SampleModel.glb'
Assert-True ($appSourceText -match 'AssetManager::GetInstance\s*\(\s*\)\.GetFbxModel\s*\(') 'break: SampleModel must load through the shared AssetManager FbxModel path'
Assert-True ($appSourceText -match 'HasMesh\s*\(\s*\)') 'break: initialization must reject an imported model with no renderable mesh'

# Break caught: a comparison or preset branch disappears, or capture defaults cease to be deterministic.
Assert-True ($appHeaderText -match '(?s)enum\s+class\s+RenderMode[^\{]*\{\s*Pbr\s*,\s*ToonPbr\s*,\s*Split\s*\}') 'break: RenderMode must expose Pbr, ToonPbr, and Split'
Assert-True ($appHeaderText -match '(?s)enum\s+class\s+LightingPreset[^\{]*\{\s*NeonContrast\s*,\s*IndustrialSoft\s*\}') 'break: LightingPreset must expose NeonContrast and IndustrialSoft'
Assert-True ($appHeaderText -match 'm_renderMode\s*=\s*RenderMode::ToonPbr') 'break: Hybrid Toon-PBR must remain the default render mode'
Assert-True ($appHeaderText -match 'm_lightingPreset\s*=\s*LightingPreset::NeonContrast') 'break: Neon Contrast must remain the default lighting preset'
Assert-True ($appSourceText -match 'ReadmeCapture::IsEnabled\s*\(\s*\)') 'break: README capture mode must be detected explicitly'
Assert-True ($appSourceText -match 'SetAnimationTimeSeconds\s*\(\s*kCapturePoseTimeSeconds\s*\)') 'break: README capture must use a fixed animation time'
Assert-True ($appSourceText -match 'SetAnimationPlaying\s*\(\s*!\s*m_readmeCapture\s*\)') 'break: interactive animation must remain active outside capture mode'
Assert-True ($appSourceText -match 'UpdateAnimation\s*\(') 'break: the selected embedded animation must update through FbxModel'
Assert-True ($appSourceText -match 'm_poseAnimator->Initialize\s*\(') 'break: Project 38 must initialize its project-local embedded-clip evaluator'
Assert-True ($appSourceText -match 'm_poseAnimator->UploadPalette\s*\(') 'break: the selected embedded pose must be uploaded every frame'
Assert-True ($appSourceText -match 'CopyResource\s*\(\s*m_character->GetBoneConstantBuffer\s*\(\s*\)\s*,\s*m_poseAnimator->GetBoneCB\s*\(\s*\)\s*\)') 'break: the evaluated pose must populate the FbxModel-owned bone palette'

# Break caught: the skinned vertex/bone interface diverges from Common::VertexSkinnedTBN.
foreach ($layoutContract in @(
    '"POSITION"[^\r\n]+DXGI_FORMAT_R32G32B32_FLOAT',
    '"NORMAL"[^\r\n]+DXGI_FORMAT_R32G32B32_FLOAT',
    '"TANGENT"[^\r\n]+DXGI_FORMAT_R32G32B32_FLOAT',
    '"BINORMAL"[^\r\n]+DXGI_FORMAT_R32G32B32_FLOAT',
    '"COLOR"[^\r\n]+DXGI_FORMAT_R32G32B32A32_FLOAT',
    '"TEXCOORD"[^\r\n]+DXGI_FORMAT_R32G32_FLOAT',
    '"BLENDINDICES"[^\r\n]+DXGI_FORMAT_R16G16B16A16_UINT',
    '"BLENDWEIGHT"[^\r\n]+DXGI_FORMAT_R32G32B32A32_FLOAT'
)) {
    Assert-True ($appSourceText -match $layoutContract) "break: skinned TBN input-layout entry missing or wrong: $layoutContract"
}
Assert-True ($appSourceText -match 'sizeof\s*\(\s*VertexSkinnedTBN\s*\)') 'break: character vertex stride must agree with Common VertexSkinnedTBN'
Assert-True ($appSourceText -match 'GetBoneConstantBuffer\s*\(\s*\)') 'break: the FbxModel bone palette must be bound to the skinned shader'

# Break caught: resize/depth formats regress or a render target is rebound while still exposed as an SRV.
foreach ($format in @(
    'DXGI_FORMAT_R16G16B16A16_FLOAT',
    'DXGI_FORMAT_R32_TYPELESS',
    'DXGI_FORMAT_D32_FLOAT',
    'DXGI_FORMAT_R32_FLOAT',
    'DXGI_FORMAT_R8_UNORM'
)) {
    Assert-True ($appSourceText -match "\b$format\b") "break: required render-resource format missing: $format"
}
Assert-True ([regex]::Matches($appSourceText, '\bDXGI_FORMAT_R32_TYPELESS\b').Count -ge 2) 'break: both main depth and shadow map must use typeless R32 textures'
Assert-True ($appSourceText -match 'PSSetShaderResources\s*\([^;]+nullShaderResources') 'break: shader resources must be unbound before RTV/DSV reuse'
Assert-True ($appSourceText -match 'ReleaseWindowSizeResources\s*\(\s*\)') 'break: resize must release and recreate window-sized HDR/normal/depth/outline resources'

# Break caught: deterministic SampleModel material profiles or name-based fallback classification disappear.
Assert-True ($appSourceText -match '\bkSampleModelMaterialOverrides\b') 'break: SampleModel must keep an explicit per-material profile override table'
Assert-True ([regex]::Matches($appSourceText, '\{\s*\d+\s*,\s*MaterialProfile::(?:Skin|Hair|Cloth)\s*\}').Count -eq 13) 'break: every SampleModel material index must have one deterministic profile override'
foreach ($classificationTerm in @('hair', 'face', 'skin', 'cloth', 'body')) {
    Assert-True ($appSourceText -match $classificationTerm) "break: case-insensitive material-name classifier lost term: $classificationTerm"
}

# Break caught: split comparison changes scene inputs or leaks half-screen viewport/scissor state into post passes.
$characterPassBody = Get-CppFunctionBody $appSourceText 'void\s+App::RenderCharacterPass\s*\([^)]*\)'
Assert-True ($characterPassBody -match 'RenderMode::Split') 'break: the character pass must implement same-frame split comparison'
Assert-True ($characterPassBody -match 'RenderMode::Pbr') 'break: split left half must use baseline PBR'
Assert-True ($characterPassBody -match 'RenderMode::ToonPbr') 'break: split right half must use Hybrid Toon-PBR'
Assert-True ($characterPassBody -match 'RSSetScissorRects') 'break: split comparison must confine both half-width draws with scissors'
Assert-True ($characterPassBody -match 'SetFullScreenViewportAndScissor\s*\(\s*\)') 'break: split comparison must restore full-screen viewport/scissor state'

# Break caught: measured pass labels cease to bracket the real pass calls.
$onRenderBody = Get-CppFunctionBody $appSourceText 'void\s+App::OnRender\s*\(\s*\)'
foreach ($pass in @('Shadow', 'Character', 'Outline', 'ToneMap')) {
    $passCall = if ($pass -eq 'ToneMap') { 'RenderToneMapPass' } else { "Render${pass}Pass" }
    Assert-True ($onRenderBody -match ("(?s)BeginPass\s*\([^;]+GpuPass::{0}\s*\).*?{1}\s*\(.*?EndPass\s*\([^;]+GpuPass::{0}\s*\)" -f $pass, $passCall)) "break: GpuProfiler must bracket the real $pass pass"
}
Assert-True ($onRenderBody -match 'BeginFrame\s*\(') 'break: measured render work must begin one GPU query frame'
Assert-True ($onRenderBody -match 'EndFrame\s*\(') 'break: measured render work must end the matching GPU query frame'
Assert-True ($onRenderBody -match 'Resolve\s*\(') 'break: completed query slots must be resolved without blocking the current frame'

# Break caught: shader outputs/features are simplified until the showcase contract is no longer observable.
Assert-True ($characterVertexShaderText -match '#include\s+"38_Shared\.fxh"') 'break: character VS must share the C++/HLSL buffer contract'
Assert-True ($characterVertexShaderText -match '\bcbBones\b') 'break: character VS must skin from the FbxModel bone palette'
Assert-True ($characterVertexShaderText -match '\bVSShadow\b') 'break: the shadow pass must use the same skinned pose'
Assert-True ($fullscreenVertexShaderText -match 'SV_VertexID') 'break: fullscreen post passes must use a cached vertex-free triangle'

Assert-True ($characterPixelShaderText -match 'SV_TARGET0') 'break: character PS must emit HDR color to MRT0'
Assert-True ($characterPixelShaderText -match 'SV_TARGET1') 'break: character PS must emit encoded world normal/profile to MRT1'
foreach ($feature in @(
    'diffuseBandThresholds',
    'bandSoftness',
    'coolShadowTint',
    'warmKeyTint',
    'materialProfile',
    'hairBand',
    'rimTerm',
    'alphaCutoff'
)) {
    Assert-True ($characterPixelShaderText -match "\b$feature\b") "break: Hybrid Toon-PBR shader feature missing: $feature"
}
Assert-True ($characterPixelShaderText -match 'clip\s*\([^;]*alphaCutoff') 'break: masked character materials must retain alpha clipping'
Assert-True ($characterPixelShaderText -match '\bshadowMap\b') 'break: character shading must consume the measured shadow map'

Assert-True ($outlinePixelShaderText -match '\bnormalProfileTexture\b') 'break: outline detection must sample encoded world normals/profiles'
Assert-True ($outlinePixelShaderText -match '\bdepthTexture\b') 'break: outline detection must sample main depth'
Assert-True ($outlinePixelShaderText -match '\boutlineQuality\b') 'break: outline shader must expose two measurable quality levels'
Assert-True ($outlinePixelShaderText -match '\binverseResolution\b') 'break: outline width must scale in pixels with resolution'
Assert-True ($outlinePixelShaderText -match 'depth[^;\r\n]*normal|normal[^;\r\n]*depth') 'break: outline mask must combine normal and depth discontinuities'

Assert-True ($toneMapPixelShaderText -match '\blightingPreset\b') 'break: tone mapping must select the active original lighting preset'
Assert-True ($toneMapPixelShaderText -match '\bexposure\b') 'break: tone mapping must apply the shared exposure contract'
Assert-True ($toneMapPixelShaderText -match '\boutlineMaskTexture\b') 'break: tone mapping must composite the outline mask'
Assert-True ($toneMapPixelShaderText -match '\bmaterialAwareOutlineColor\b') 'break: outline composite must use material profile rather than unconditional black'
Assert-True ($toneMapPixelShaderText -match '\bNeonContrast\b') 'break: Neon Contrast tone response must remain present'
Assert-True ($toneMapPixelShaderText -match '\bIndustrialSoft\b') 'break: Industrial Soft tone response must remain present'
Assert-True ($sharedShaderText -match '\bMaterialProfile') 'break: material profile values must be shared across character and post shaders'

# Break caught: approved HUD prose expands/changes, or timings become placeholders instead of real samples/states.
Assert-True ($appSourceText -match 'ImGui::TextUnformatted\s*\(\s*"Hybrid Toon-PBR Character Showcase"\s*\)') 'break: approved compact HUD title line changed'
Assert-True ($appSourceText -match 'ImGui::TextUnformatted\s*\(\s*"Material-aware toon shading, hair highlights, rim lighting, stable outlines, and GPU cost comparison\."\s*\)') 'break: approved compact HUD description line changed'
Assert-True ($appSourceText -match 'std::chrono::') 'break: HUD CPU milliseconds must come from a real measured frame interval'
Assert-True ($appSourceText -match 'Latest\s*\(\s*\)') 'break: HUD GPU total/pass values must consume GpuProfiler results'
Assert-True ($appSourceText -match 'GPU: warming up') 'break: HUD must label profiler warm-up honestly'
Assert-True ($appSourceText -match 'GPU: unavailable') 'break: HUD must continue with an honest unavailable state if profiler initialization fails'
foreach ($passLabel in @('Shadow', 'Character', 'Outline', 'ToneMap')) {
    Assert-True ($appSourceText -match ("{0}.*?ms" -f $passLabel)) "break: HUD must expose measured $passLabel milliseconds"
}
foreach ($control in @('Mode', 'Preset', 'Band thresholds', 'Band softness', 'Shadow tint', 'Key tint', 'Hair highlight', 'Rim strength', 'Shadow softness', 'Outline width', 'Outline quality', 'Exposure')) {
    Assert-True ($appSourceText -match [regex]::Escape($control)) "break: runtime control missing from compact HUD: $control"
}

$rootReadmeText = Get-Content -Raw -LiteralPath $rootReadmePath
$project37ReadmeText = Get-Content -Raw -LiteralPath $project37ReadmePath
$readmeText = Get-Content -Raw -LiteralPath $readmePath

# Break caught: the Project 37 -> 38 chain, terminal Project 38 navigation, or
# the standard generated README marker/media structure regresses.
$documentationContractFailures = [Collections.Generic.List[string]]::new()
function Assert-DocumentationContract([bool]$Condition, [string]$Message) {
    if (-not $Condition) { $script:documentationContractFailures.Add($Message) }
}

Assert-DocumentationContract ([regex]::Matches($project37ReadmeText, '\[다음\]\(\.\./38_StylizedToonPBR/README\.md\)').Count -eq 2) 'docs: Project 37 top and bottom next links must point to Project 38'
Assert-DocumentationContract ([regex]::Matches($readmeText, '\[이전\]\(\.\./37_Blueprint/README\.md\)').Count -eq 2) 'docs: Project 38 top and bottom previous links must point to Project 37'
Assert-DocumentationContract ([regex]::Matches($readmeText, '(?m)^\[이전\]\(\.\./37_Blueprint/README\.md\) \| \[메인\]\(\.\./\.\./README\.md\) \| \[상위\]\(\.\./\) \| 다음$').Count -eq 2) 'docs: Project 38 must use the plain terminal next label in both navigation blocks'

$standardMarkers = @(
    '<!-- README-NAV-TOP:START -->',
    '<!-- README-NAV-TOP:END -->',
    '<!-- README-INFO:START -->',
    '<!-- README-INFO:END -->',
    '<!-- README-RUNTIME:START -->',
    '<!-- README-RUNTIME:END -->',
    '<!-- README-NAV-BOTTOM:START -->',
    '<!-- README-NAV-BOTTOM:END -->'
)
foreach ($marker in $standardMarkers) {
    Assert-DocumentationContract ([regex]::Matches($readmeText, [regex]::Escape($marker)).Count -eq 1) "docs: Project 38 standard marker missing or duplicated: $marker"
}
for ($markerIndex = 1; $markerIndex -lt $standardMarkers.Count; ++$markerIndex) {
    Assert-DocumentationContract ($readmeText.IndexOf($standardMarkers[$markerIndex - 1]) -lt $readmeText.IndexOf($standardMarkers[$markerIndex])) "docs: Project 38 standard markers are out of order near $($standardMarkers[$markerIndex])"
}
Assert-DocumentationContract ($readmeText -match '(?m)^#\s+38\.\s+Stylized Toon PBR\s*$') 'docs: Project 38 polished detail heading missing'
Assert-DocumentationContract ($readmeText -match '\.\./\.\./docs/media/readme/info/38-StylizedToonPBR-info\.png') 'docs: Project 38 information-image marker must use the manifest path'
Assert-DocumentationContract ($readmeText -match '\.\./\.\./docs/media/readme/38-StylizedToonPBR\.png') 'docs: Project 38 runtime PNG must use the manifest path'
Assert-DocumentationContract ($readmeText -match '\.\./\.\./docs/media/readme/38-StylizedToonPBR\.gif') 'docs: Project 38 runtime GIF must use the manifest path'

# Break caught: the guide no longer explains the implemented comparison,
# material-aware passes, exact HUD controls, degraded states, or measurement
# evidence needed to reproduce and interpret the showcase.
foreach ($mode in @('PBR', 'Hybrid Toon-PBR', 'Split')) {
    Assert-DocumentationContract ($readmeText -match [regex]::Escape($mode)) "docs: Project 38 comparison mode missing: $mode"
}
foreach ($preset in @('Neon Contrast', 'Industrial Soft')) {
    Assert-DocumentationContract ($readmeText -match [regex]::Escape($preset)) "docs: Project 38 lighting preset missing: $preset"
}
foreach ($profile in @('Skin', 'Hair', 'Cloth')) {
    Assert-DocumentationContract ($readmeText -match "(?i)\b$profile\b") "docs: Project 38 material profile missing: $profile"
}
foreach ($technique in @('Shadow Map', 'Normal/Depth', 'Tone Mapping')) {
    Assert-DocumentationContract ($readmeText -match [regex]::Escape($technique)) "docs: Project 38 measured render-pass explanation missing: $technique"
}
foreach ($control in @('Band thresholds', 'Band softness', 'Shadow tint', 'Key tint', 'Hair highlight', 'Rim strength', 'Shadow softness', 'Outline width', 'Outline quality', 'Exposure')) {
    Assert-DocumentationContract ($readmeText -match [regex]::Escape($control)) "docs: Project 38 exact HUD control missing: $control"
}
Assert-DocumentationContract ($readmeText -match '(?i)4[- ]slot') 'docs: Project 38 must explain the four-slot GPU query ring'
Assert-DocumentationContract ($readmeText -match 'D3D11_ASYNC_GETDATA_DONOTFLUSH') 'docs: Project 38 must explain the non-flushing GPU result read'
Assert-DocumentationContract ($readmeText -match '2\s*프레임') 'docs: Project 38 must explain the two-frame delayed query resolve latency'
Assert-DocumentationContract ($readmeText -match 'warming up') 'docs: Project 38 must document the profiler warm-up state'
Assert-DocumentationContract ($readmeText -match 'unavailable') 'docs: Project 38 must document the unavailable profiler state'
Assert-DocumentationContract ($readmeText -match '(?i)Outlines disabled') 'docs: Project 38 must document graceful outline disablement'
Assert-DocumentationContract ($readmeText -match '(?m)^###\s+실행 코드에서 확인되는 측정 근거\s*$') 'docs: Project 38 must separate implemented measurement evidence'
Assert-DocumentationContract ($readmeText -match '(?m)^###\s+측정값 해석과 일반 조언\s*$') 'docs: Project 38 must separate general optimization advice from evidence'
Assert-DocumentationContract ($readmeText -notmatch '\b\d+(?:\.\d+)?\s*ms\b') 'docs: Project 38 must not invent fixed benchmark millisecond results'

# Break caught: Project 38 disappears from the root directory/gallery or its
# focused stylized-rendering showcase is misplaced before the Project 36 demo.
$project36DemoIndex = $rootReadmeText.IndexOf('## 대표 데모')
$stylizedShowcaseIndex = $rootReadmeText.IndexOf('## 스타일라이즈드 렌더링 쇼케이스')
$projectShortcutsIndex = $rootReadmeText.IndexOf('### 프로젝트 바로가기')
Assert-DocumentationContract ($rootReadmeText -match 'Dx11/38_StylizedToonPBR') 'docs: root README must link the Project 38 directory'
Assert-DocumentationContract ($rootReadmeText -match 'docs/media/readme/38-StylizedToonPBR\.png') 'docs: root README must include the Project 38 PNG gallery image'
Assert-DocumentationContract ($project36DemoIndex -ge 0 -and $stylizedShowcaseIndex -gt $project36DemoIndex -and $stylizedShowcaseIndex -lt $projectShortcutsIndex) 'docs: stylized-rendering showcase must follow the Project 36 representative demo and precede project shortcuts'

# Break caught: branding prevention silently covers fewer than the exact 38
# manifest-selected detail READMEs, or a centered mascot block returns under
# a renamed marker.
$projectReadmePaths = @($manifest.projects | ForEach-Object { Join-Path $repoRoot "Dx11/$($_.directory)/README.md" })
Assert-DocumentationContract ($projectReadmePaths.Count -eq 38) 'docs: branding scope must contain exactly 38 project READMEs'
foreach ($projectReadmePath in $projectReadmePaths) {
    $projectReadmeText = Get-Content -Raw -LiteralPath $projectReadmePath
    Assert-DocumentationContract ($projectReadmeText -notmatch 'README-BRAND:(?:START|END)') "docs: README-BRAND marker returned: $projectReadmePath"
    Assert-DocumentationContract ($projectReadmeText -notmatch 'alice-tutorial-logo\.png') "docs: detailed README mascot logo returned: $projectReadmePath"
    $centeredMascotPattern = '(?is)<(?:p|div)\s+align="center"[^>]*>\s*(?:<a[^>]*>\s*)?<img[^>]*(?:alice-tutorial-logo\.png|mascot\s+logo)[^>]*>(?:\s*</a>)?\s*</(?:p|div)>'
    Assert-DocumentationContract ($projectReadmeText -notmatch $centeredMascotPattern) "docs: centered mascot image block returned: $projectReadmePath"
}

if ($documentationContractFailures.Count -gt 0) {
    throw ("Documentation contracts failed:`n - " + ($documentationContractFailures -join "`n - "))
}

'Project 38 structure, renderer, shader, HUD, and GPU-profiler contract tests passed'
