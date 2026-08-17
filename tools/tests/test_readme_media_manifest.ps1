$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\readme_media_common.ps1')

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifest = Get-ReadmeMediaManifest -ManifestPath 'tools/readme_media_manifest.json' -RepoRoot $repoRoot
$errors = @(Test-ReadmeMediaManifest -Manifest $manifest -RepoRoot $repoRoot)

Assert-True ($errors.Count -eq 0) ($errors -join "`n")
Assert-True ($manifest.expectedProjectCount -eq 37) 'production manifest expectedProjectCount must be 37'
Assert-True (@($manifest.projects).Count -eq 37) 'manifest must contain 37 projects'
Assert-True (-not (@($manifest.projects.directory) -contains '16_pmxWithMotion')) 'duplicate project must stay excluded'
Assert-True ($manifest.captureWidth -eq 1600 -and $manifest.captureHeight -eq 900) 'PNG size contract mismatch'
Assert-True ($manifest.gifWidth -eq 800 -and $manifest.gifHeight -eq 450) 'GIF size contract mismatch'
Assert-True ($manifest.infoWidth -eq 1600 -and $manifest.infoHeight -eq 640) 'info image size contract mismatch'

$project36 = @($manifest.projects | Where-Object number -eq '36')[0]
Assert-True ((Get-ReadmeMediaEffectivePositiveNumber $manifest $project36 'gifSeconds') -eq 13) 'project 36 must capture thirteen seconds'
Assert-True ([bool]$project36.readmeBackbufferCapture) 'project 36 must opt into backbuffer capture'

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
