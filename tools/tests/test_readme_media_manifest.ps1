$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\readme_media_common.ps1')

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifest = Get-ReadmeMediaManifest -ManifestPath 'tools/readme_media_manifest.json' -RepoRoot $repoRoot
$errors = @(Test-ReadmeMediaManifest -Manifest $manifest -RepoRoot $repoRoot)

Assert-True ($errors.Count -eq 0) ($errors -join "`n")
Assert-True ($manifest.expectedProjectCount -eq 38) 'production manifest expectedProjectCount must be 38'
Assert-True (@($manifest.projects).Count -eq 38) 'manifest must contain 38 projects'
Assert-True (-not (@($manifest.projects.directory) -contains '16_pmxWithMotion')) 'duplicate project must stay excluded'
Assert-True ($manifest.captureWidth -eq 1600 -and $manifest.captureHeight -eq 900) 'PNG size contract mismatch'
Assert-True ($manifest.gifWidth -eq 800 -and $manifest.gifHeight -eq 450) 'GIF size contract mismatch'
Assert-True ($manifest.infoWidth -eq 1600 -and $manifest.infoHeight -eq 640) 'info image size contract mismatch'

$captureModeProjects = @($manifest.projects | Where-Object { $_.readmeCaptureMode } | ForEach-Object { $_.number })
$expectedCaptureModeProjects = @(
    '06','07','11','12','13','15','16','17','18','19','20','21','22','23','24','25','26','27','28','29','30','31','32','33','34','35','36','38'
)
Assert-True (($captureModeProjects -join ',') -ceq ($expectedCaptureModeProjects -join ',')) `
    "README capture-mode project selection changed: $($captureModeProjects -join ',')"

$presentationPanProjects = @($manifest.projects | Where-Object { $_.gifPresentationPan } | ForEach-Object { $_.number })
Assert-True (($presentationPanProjects -join ',') -ceq '01,06,28,33,37') `
    "presentation-pan project selection changed: $($presentationPanProjects -join ',')"

$shortSymmetricMotionProjects = @()
$expectedShortMotionSignature = '400:keyDown:D,650:keyUp:D,2200:keyDown:A,2450:keyUp:A'
foreach ($project in $manifest.projects) {
    $signature = @($project.gifActions | ForEach-Object { "$($_.atMs):$($_.type):$($_.key)" }) -join ','
    if ($signature -ceq $expectedShortMotionSignature) {
        $shortSymmetricMotionProjects += $project.number
    }
}
Assert-True (($shortSymmetricMotionProjects -join ',') -ceq '07,18,21,22,24') `
    "short symmetric runtime-motion action selection changed: $($shortSymmetricMotionProjects -join ',')"

$project36 = @($manifest.projects | Where-Object number -eq '36')[0]
Assert-True ((Get-ReadmeMediaEffectivePositiveNumber $manifest $project36 'gifSeconds') -eq 13) 'project 36 must capture thirteen seconds'
Assert-True ([bool]$project36.readmeBackbufferCapture) 'project 36 must opt into backbuffer capture'

$skyboxPreflightProjects = @($manifest.projects | Where-Object { $_.skyboxIblSet } | ForEach-Object { $_.number })
Assert-True (($skyboxPreflightProjects -join ',') -ceq '31,32,33,34,35,36') `
    "IBL capture preflight selection changed: $($skyboxPreflightProjects -join ',')"
Assert-True (@($manifest.projects | Where-Object { $_.skyboxIblSet -and $_.skyboxIblSet -cne 'Sample' }).Count -eq 0) `
    'the current IBL capture projects must preflight their Sample set'

$project06 = @($manifest.projects | Where-Object number -eq '06')[0]
Assert-True ([bool]$project06.readmeCaptureMode) 'project 06 must opt into deterministic README capture mode'
Assert-True ($null -ne $project06.PSObject.Properties['minSampledPngColors']) 'project 06 must declare its low-colour PNG floor'
Assert-True ([int]$project06.minSampledPngColors -eq 2) `
    'project 06 low-colour PNG floor must match the fresh foreground/background evidence'
Assert-True (@($manifest.projects | Where-Object { $null -ne $_.PSObject.Properties['minSampledPngColors'] }).Count -eq 1) `
    'only project 06 may declare a low-colour PNG floor'

foreach ($project in @($manifest.projects | Where-Object number -ne '36')) {
    Assert-True ((Get-ReadmeMediaEffectivePositiveNumber $manifest $project 'gifSeconds') -eq 4) "project $($project.number) changed from four seconds"
}

foreach ($project in $manifest.projects) {
    Assert-True ($project.number -match '^\d{2}$') "invalid number: $($project.number)"
    Assert-True (-not [string]::IsNullOrWhiteSpace($project.title)) "missing title: $($project.number)"
    Assert-True (-not [string]::IsNullOrWhiteSpace($project.summary)) "missing summary: $($project.number)"
    Assert-True (@($project.tags).Count -ge 3 -and @($project.tags).Count -le 5) "invalid tags: $($project.number)"
    Assert-True ($project.image -match '\.png$') "missing PNG path: $($project.number)"
    Assert-True ($project.gif -match '\.gif$') "missing GIF path: $($project.number)"
    Assert-True ($project.infoImage -match '^info/.+-info\.png$') "missing info image path: $($project.number)"
}

function Copy-ReadmeMediaManifest([object]$SourceManifest) {
    return $SourceManifest | ConvertTo-Json -Depth 10 | ConvertFrom-Json
}

$invalid = Copy-ReadmeMediaManifest $manifest
$invalid.projects[35].gifSeconds = 0
Assert-True ((Test-ReadmeMediaManifest $invalid $repoRoot) -contains 'invalid gifSeconds override: 36') 'zero override was accepted'

$invalidSkyboxSet = Copy-ReadmeMediaManifest $manifest
$invalidSkyboxSet.projects[30].skyboxIblSet = 'Sample/../../outside'
Assert-True ((Test-ReadmeMediaManifest $invalidSkyboxSet $repoRoot) -contains 'invalid skyboxIblSet: 31') `
    'unsafe or unsupported skybox IBL set was accepted'

$malformedPngFloor = Copy-ReadmeMediaManifest $manifest
$malformedPngFloor.projects[5].minSampledPngColors = 'four'
Assert-True ((Test-ReadmeMediaManifest $malformedPngFloor $repoRoot) -contains 'invalid minSampledPngColors override: 06') `
    'malformed project 06 PNG floor was accepted'

$weakPngFloor = Copy-ReadmeMediaManifest $manifest
$weakPngFloor.projects[5].minSampledPngColors = 1
Assert-True ((Test-ReadmeMediaManifest $weakPngFloor $repoRoot) -contains 'minSampledPngColors override is too weak: 06') `
    'overly weak project 06 PNG floor was accepted'

$redundantPngFloor = Copy-ReadmeMediaManifest $manifest
$redundantPngFloor.projects[5].minSampledPngColors = 8
Assert-True ((Test-ReadmeMediaManifest $redundantPngFloor $repoRoot) -contains 'minSampledPngColors override must stay below the global floor: 06') `
    'project 06 PNG floor did not remain a narrow exception'

$unrelatedPngFloor = Copy-ReadmeMediaManifest $manifest
$unrelatedPngFloor.projects[6] | Add-Member -NotePropertyName minSampledPngColors -NotePropertyValue 2
Assert-True ((Test-ReadmeMediaManifest $unrelatedPngFloor $repoRoot) -contains 'minSampledPngColors override is restricted to project 06: 07') `
    'an unrelated project was allowed to weaken the PNG floor'

$regressionFailures = [System.Collections.Generic.List[string]]::new()
foreach ($invalidAtMs in @('NaN', 'Infinity', '-Infinity')) {
    $invalidManifest = Copy-ReadmeMediaManifest $manifest
    $invalidManifest.projects[2].gifActions[0].atMs = $invalidAtMs
    $invalidErrors = @(Test-ReadmeMediaManifest -Manifest $invalidManifest -RepoRoot $repoRoot)
    if ($invalidErrors -notcontains 'malformed action timing: 03') {
        $null = $regressionFailures.Add("$invalidAtMs must be rejected as malformed action timing")
    }
}

$fractionalManifest = Copy-ReadmeMediaManifest $manifest
$fractionalManifest.captureWidth = 1600.5
$fractionalErrors = @(Test-ReadmeMediaManifest -Manifest $fractionalManifest -RepoRoot $repoRoot)
if ($fractionalErrors -notcontains 'invalid manifest value: captureWidth') {
    $null = $regressionFailures.Add('fractional captureWidth must be rejected')
}

foreach ($invalidPath in @(
    @{ Property = 'exe'; Value = '..\outside.exe' },
    @{ Property = 'image'; Value = '..\outside.png' },
    @{ Property = 'gif'; Value = '..\outside.gif' },
    @{ Property = 'infoImage'; Value = 'info\..\outside-info.png' },
    @{ Property = 'directory'; Value = '..\outside-project' },
    @{ Property = 'exe'; Value = 'C:\outside.exe' }
)) {
    $invalidManifest = Copy-ReadmeMediaManifest $manifest
    $invalidManifest.projects[0].($invalidPath.Property) = $invalidPath.Value
    $invalidErrors = @(Test-ReadmeMediaManifest -Manifest $invalidManifest -RepoRoot $repoRoot)
    $expectedError = "invalid safe relative path '$($invalidPath.Property)': 01"
    if ($invalidErrors -notcontains $expectedError) {
        $null = $regressionFailures.Add("$($invalidPath.Property) path '$($invalidPath.Value)' must be rejected")
    }
}

Assert-True ($regressionFailures.Count -eq 0) ($regressionFailures -join "`n")

$containedImage = Resolve-ReadmeMediaContainedPath -BasePath (Join-Path $repoRoot 'docs\media\readme') -Path '01-fixture.png' -Description 'image output'
Assert-True ($containedImage -eq (Join-Path $repoRoot 'docs\media\readme\01-fixture.png')) 'contained media path was resolved incorrectly'

$traversalRejected = $false
try {
    $null = Resolve-ReadmeMediaContainedPath -BasePath (Join-Path $repoRoot 'docs\media\readme') -Path '..\outside.png' -Description 'image output'
}
catch {
    $traversalRejected = $_.Exception.Message -match 'safe relative path'
}
Assert-True $traversalRejected 'capture path containment accepted traversal'

'manifest contract tests passed'
