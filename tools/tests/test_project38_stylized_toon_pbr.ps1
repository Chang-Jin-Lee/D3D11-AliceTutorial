$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$projectName = '38_StylizedToonPBR'
$projectGuid = '{4B6A5522-0C57-41E0-A222-6AA813BBCE5C}'
$projectDirectory = Join-Path $repoRoot "Dx11\$projectName"
$projectPath = Join-Path $projectDirectory "$projectName.vcxproj"
$filtersPath = "$projectPath.filters"
$readmePath = Join-Path $projectDirectory 'README.md'

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
    $readmePath
)) {
    Assert-True (Test-Path -LiteralPath $path -PathType Leaf) "Project 38 shell file missing: $path"
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
Assert-True ((@($compileFiles | Sort-Object) -join ',') -ceq (@('App.cpp', 'WinMain.cpp') -join ',')) 'Project 38 vcxproj must declare exactly the Task 2 C++ sources'
Assert-True ((@($includeFiles | Sort-Object) -join ',') -ceq (@('App.h') -join ',')) 'Project 38 vcxproj must declare exactly the Task 2 header'
Assert-True (@($projectXml.SelectNodes('//msb:FxCompile | //msb:CopyFileToFolders | //msb:None[contains(@Include, ".hlsl") or contains(@Include, ".hlsli") or contains(@Include, ".fxh")]', $namespace)).Count -eq 0) 'Task 2 must not declare runtime HLSL files'

$projectText = Get-Content -Raw -LiteralPath $projectPath
Assert-True ($projectText -match '\$\(SolutionDir\)Resource\\\*') 'Project 38 must copy the public Resource directory'
Assert-True ($projectText -match '\$\(OutDir\)\\\.\.\\Resource\\') 'Project 38 runtime Resource destination mismatch'

[xml]$filtersXml = Get-Content -Raw -LiteralPath $filtersPath
$filtersNamespace = [Xml.XmlNamespaceManager]::new($filtersXml.NameTable)
$filtersNamespace.AddNamespace('msb', 'http://schemas.microsoft.com/developer/msbuild/2003')
$filteredCompiles = @($filtersXml.SelectNodes('//msb:ClCompile', $filtersNamespace) | ForEach-Object { $_.Include })
$filteredIncludes = @($filtersXml.SelectNodes('//msb:ClInclude', $filtersNamespace) | ForEach-Object { $_.Include })
Assert-True ((@($filteredCompiles | Sort-Object) -join ',') -ceq (@('App.cpp', 'WinMain.cpp') -join ',')) 'Project 38 filters must declare exactly the Task 2 C++ sources'
Assert-True ((@($filteredIncludes | Sort-Object) -join ',') -ceq (@('App.h') -join ',')) 'Project 38 filters must declare exactly the Task 2 header'

$readmeText = Get-Content -Raw -LiteralPath $readmePath
Assert-True ($readmeText -match '(?m)^#\s+Project 38') 'Project 38 skeletal README heading missing'
Assert-True ($readmeText -notmatch 'README-BRAND:(?:START|END)') 'Project 38 README brand markers must be absent'
Assert-True ($readmeText -notmatch 'alice-tutorial-logo\.png') 'Project 38 README logo reference must be absent'

'Project 38 structure and registration contract tests passed'
