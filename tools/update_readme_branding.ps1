param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Manifest = 'tools/readme_media_manifest.json'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = [IO.Path]::GetFullPath($RepoRoot)
$manifestPath = if ([IO.Path]::IsPathRooted($Manifest)) { $Manifest } else { Join-Path $root $Manifest }
$data = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$solutionPath = Join-Path $root 'Dx11\TutorialApp.sln'
if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) { throw "solution missing: $solutionPath" }
$solutionText = Get-Content -Raw -LiteralPath $solutionPath
$solutionProjectNames = @(
    [regex]::Matches($solutionText, '(?m)^Project\("[^"]+"\) = "([^"]+)", "([^"]+\.vcxproj)"') |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -ne 'Common' }
)
if ($solutionProjectNames.Count -ne 37) { throw "solution must contain exactly 37 application projects, got $($solutionProjectNames.Count)" }
if (@($solutionProjectNames | Sort-Object -Unique).Count -ne 37) { throw 'solution application projects must be unique' }
foreach ($solutionProjectName in $solutionProjectNames) {
    if ($solutionProjectName -notmatch '^\d{2}_[A-Za-z0-9_]+$') { throw "invalid solution application project name: $solutionProjectName" }
}

$manifestProjects = @($data.projects)
if ([int]$data.expectedProjectCount -ne 37 -or $manifestProjects.Count -ne 37) { throw 'manifest project count mismatch' }
$manifestDirectories = @($manifestProjects | ForEach-Object { [string]$_.directory })
foreach ($directory in $manifestDirectories) {
    if ($directory -notmatch '^\d{2}_[A-Za-z0-9_]+$') { throw "unsafe project directory: $directory" }
}
if (@($manifestDirectories | Sort-Object -Unique).Count -ne 37) { throw 'manifest project directories must be unique' }
$sortedManifestDirectories = @($manifestDirectories | Sort-Object -CaseSensitive)
$sortedSolutionProjectNames = @($solutionProjectNames | Sort-Object -CaseSensitive)
if (($sortedManifestDirectories -join "`n") -cne ($sortedSolutionProjectNames -join "`n")) {
    throw 'manifest project directories must exactly match TutorialApp.sln application projects'
}

function New-BrandBlock([string]$ImagePath, [int]$Width, [string]$Newline) {
    return @(
        '<!-- README-BRAND:START -->',
        "<p align=`"center`"><img src=`"$ImagePath`" width=`"$Width`" alt=`"D3D11 Alice Tutorial mascot logo`" /></p>",
        '<!-- README-BRAND:END -->'
    ) -join $Newline
}

function Read-ReadmeContent([string]$Path) {
    try {
        return [IO.File]::ReadAllText($Path, [Text.UTF8Encoding]::new($false, $true))
    }
    catch [Text.DecoderFallbackException] {
        $cp949 = [Text.Encoding]::GetEncoding(949, [Text.EncoderFallback]::ExceptionFallback, [Text.DecoderFallback]::ExceptionFallback)
        return [IO.File]::ReadAllText($Path, $cp949)
    }
}

function Get-UpdatedReadme([string]$Path, [string]$ImagePath, [int]$Width) {
    $content = Read-ReadmeContent $Path
    $newline = if ($content.Contains("`r`n")) { "`r`n" } else { "`n" }
    $start = '<!-- README-BRAND:START -->'
    $end = '<!-- README-BRAND:END -->'
    $startMatches = [regex]::Matches($content, [regex]::Escape($start))
    $endMatches = [regex]::Matches($content, [regex]::Escape($end))
    $startCount = $startMatches.Count
    $endCount = $endMatches.Count
    if ($startCount -ne $endCount -or $startCount -gt 1) { throw "malformed README-BRAND markers: $Path" }
    if ($startCount -eq 1 -and $startMatches[0].Index -gt $endMatches[0].Index) { throw "malformed README-BRAND markers: $Path" }
    $block = New-BrandBlock $ImagePath $Width $newline
    if ($startCount -eq 1) {
        $blockEnd = $endMatches[0].Index + $end.Length
        $content = $content.Remove($startMatches[0].Index, $blockEnd - $startMatches[0].Index)
    }
    $heading = [regex]::Match($content, '(?m)^#{1,6}\s+.+?(?=\r?$)')
    if (-not $heading.Success) { throw "first Markdown heading missing: $Path" }
    $insertAt = $heading.Index + $heading.Length
    $suffix = $content.Substring($insertAt)
    $suffix = [regex]::Replace($suffix, '\A(?:[ \t]*\r?\n)+', '')
    return $content.Substring(0, $insertAt) + $newline + $newline + $block + $newline + $newline + $suffix
}

$targets = [Collections.Generic.List[object]]::new()
foreach ($project in @($data.projects)) {
    $targets.Add([pscustomobject]@{ Path = Join-Path $root "Dx11/$($project.directory)/README.md"; Image = '../../docs/media/branding/alice-tutorial-logo.png'; Width = 520 })
}

$updates = foreach ($target in $targets) {
    if (-not (Test-Path -LiteralPath $target.Path)) { throw "README missing: $($target.Path)" }
    [pscustomobject]@{ Path = $target.Path; Content = Get-UpdatedReadme $target.Path $target.Image $target.Width }
}

$encoding = [Text.UTF8Encoding]::new($false)
$temporary = @()
try {
    foreach ($update in $updates) {
        $temp = $update.Path + '.brand.tmp'
        [IO.File]::WriteAllText($temp, $update.Content, $encoding)
        $temporary += [pscustomobject]@{ Temp = $temp; Destination = $update.Path }
    }
    foreach ($file in $temporary) { Move-Item -LiteralPath $file.Temp -Destination $file.Destination -Force }
}
finally {
    foreach ($file in $temporary) { if (Test-Path -LiteralPath $file.Temp) { Remove-Item -LiteralPath $file.Temp -Force } }
}
