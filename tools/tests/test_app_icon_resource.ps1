$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$solutionText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\TutorialApp.sln')
$solutionProjects = @([regex]::Matches($solutionText, '(?m)^Project\("[^"]+"\) = "([^"]+)", "([^"]+\.vcxproj)"') |
    ForEach-Object { $_.Groups[1].Value } | Where-Object { $_ -ne 'Common' })
Assert-True ($solutionProjects.Count -eq 38) "expected 38 solution apps, got $($solutionProjects.Count)"

$targetsPath = Join-Path $repoRoot 'Dx11\Directory.Build.targets'
$headerPath = Join-Path $repoRoot 'Dx11\Resource\Icon\AppIconResource.h'
$rcPath = Join-Path $repoRoot 'Dx11\Resource\Icon\AppIcon.rc'
Assert-True (Test-Path -LiteralPath $targetsPath) 'Directory.Build.targets missing'
Assert-True (Test-Path -LiteralPath $headerPath) 'AppIconResource.h missing'
Assert-True (Test-Path -LiteralPath $rcPath) 'AppIcon.rc missing'

[xml]$targetsXml = Get-Content -Raw -LiteralPath $targetsPath
$allowlistNode = $targetsXml.SelectSingleNode("//*[local-name()='AliceTutorialBrandingProjects']")
Assert-True ($null -ne $allowlistNode) 'branding allowlist missing'
$allowlist = @($allowlistNode.InnerText.Split(';', [System.StringSplitOptions]::RemoveEmptyEntries))
Assert-True ($allowlist.Count -eq 38) "allowlist expected 38 apps, got $($allowlist.Count)"
Assert-True (@(Compare-Object ($solutionProjects | Sort-Object) ($allowlist | Sort-Object)).Count -eq 0) 'allowlist differs from TutorialApp.sln'

$targetsText = Get-Content -Raw -LiteralPath $targetsPath
$headerText = Get-Content -Raw -LiteralPath $headerPath
$rcText = Get-Content -Raw -LiteralPath $rcPath
$gameAppText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\Common\GameApp.cpp')
Assert-True ($targetsText -match 'ConfigurationType.*Application') 'Application guard missing'
Assert-True ($targetsText -match 'ResourceCompile.*AppIcon\.rc') 'shared ResourceCompile missing'
Assert-True ($headerText -match 'IDI_ALICE_TUTORIAL_APP_ICON\s+101') 'resource ID 101 missing'
Assert-True ($rcText -match 'AliceTutorial\.ico') 'ICO is not referenced by AppIcon.rc'
Assert-True ($gameAppText -match 'MAKEINTRESOURCEW\(IDI_ALICE_TUTORIAL_APP_ICON\)') 'embedded icon load missing'
Assert-True ($gameAppText -match 'SM_CXSMICON' -and $gameAppText -match 'hIconSm') 'small icon load missing'
Assert-True ($gameAppText -notmatch 'LR_LOADFROMFILE') 'relative icon file loading remains'
Assert-True ($gameAppText -notmatch 'Alice\.ico') 'old icon path remains in GameApp'
Assert-True (-not (Test-Path -LiteralPath (Join-Path $repoRoot 'Dx11\Resource\Icon\Alice.ico'))) 'old Alice.ico still exists'

'app icon resource contract tests passed'
