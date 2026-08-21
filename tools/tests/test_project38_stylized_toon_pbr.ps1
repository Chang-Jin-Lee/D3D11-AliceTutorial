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
$profilerHeaderPath = Join-Path $projectDirectory 'GpuProfiler.h'
$profilerSourcePath = Join-Path $projectDirectory 'GpuProfiler.cpp'

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
    (Join-Path $projectDirectory 'App.h'),
    (Join-Path $projectDirectory 'App.cpp'),
    $profilerHeaderPath,
    $profilerSourcePath,
    $readmePath
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
Assert-True (@($projectXml.SelectNodes('//msb:FxCompile | //msb:CopyFileToFolders | //msb:None[contains(@Include, ".hlsl") or contains(@Include, ".hlsli") or contains(@Include, ".fxh")]', $namespace)).Count -eq 0) 'Task 2 must not declare runtime HLSL files'

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

$readmeText = Get-Content -Raw -LiteralPath $readmePath
Assert-True ($readmeText -match '(?m)^#\s+Project 38') 'Project 38 skeletal README heading missing'
Assert-True ($readmeText -notmatch 'README-BRAND:(?:START|END)') 'Project 38 README brand markers must be absent'
Assert-True ($readmeText -notmatch 'alice-tutorial-logo\.png') 'Project 38 README logo reference must be absent'

'Project 38 structure, registration, and GPU-profiler contract tests passed'
