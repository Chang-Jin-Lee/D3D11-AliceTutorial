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

function Read-ReadmeContent([string]$Path) {
    try {
        return [IO.File]::ReadAllText($Path, [Text.UTF8Encoding]::new($false, $true))
    }
    catch [Text.DecoderFallbackException] {
        $cp949 = [Text.Encoding]::GetEncoding(949, [Text.EncoderFallback]::ExceptionFallback, [Text.DecoderFallback]::ExceptionFallback)
        return [IO.File]::ReadAllText($Path, $cp949)
    }
}

# The mascot logo used to live in a generated <!-- README-BRAND:START -->
# / <!-- README-BRAND:END --> block inserted right after the first Markdown
# heading of every project README. The author asked for the logo to be
# removed from those READMEs because it is unnecessary, so this script no
# longer builds or inserts that block. The 37 checked-in READMEs no longer
# carry the (now-empty) marker comments either -- there is nothing left for
# them to bound, and no other tool keys off them.
#
# This function only strips a README-BRAND block if one is still present,
# so that re-running this generator is durable: it can never reintroduce the
# logo, and it cleans up a block if one is ever pasted back in by hand or
# survives from an old checkout.
function Remove-LegacyBrandBlock([string]$Path, [string]$Content) {
    $newline = if ($Content.Contains("`r`n")) { "`r`n" } else { "`n" }
    $start = '<!-- README-BRAND:START -->'
    $end = '<!-- README-BRAND:END -->'
    $startMatches = [regex]::Matches($Content, [regex]::Escape($start))
    $endMatches = [regex]::Matches($Content, [regex]::Escape($end))
    $startCount = $startMatches.Count
    $endCount = $endMatches.Count
    if ($startCount -ne $endCount -or $startCount -gt 1) { throw "malformed README-BRAND markers: $Path" }
    if ($startCount -eq 0) { return $Content }
    if ($startMatches[0].Index -gt $endMatches[0].Index) { throw "malformed README-BRAND markers: $Path" }
    $blockStart = $startMatches[0].Index
    $blockEnd = $endMatches[0].Index + $end.Length
    $before = [regex]::Replace($Content.Substring(0, $blockStart), '(?:[ \t]*\r?\n)+\z', '')
    $after = [regex]::Replace($Content.Substring($blockEnd), '\A(?:[ \t]*\r?\n)+', '')
    if ($after.Length -eq 0) { return $before }
    return $before + $newline + $newline + $after
}

$targets = [Collections.Generic.List[object]]::new()
foreach ($project in @($data.projects)) {
    $targets.Add([pscustomobject]@{ Path = Join-Path $root "Dx11/$($project.directory)/README.md" })
}

$updates = foreach ($target in $targets) {
    if (-not (Test-Path -LiteralPath $target.Path)) { throw "README missing: $($target.Path)" }
    $original = Read-ReadmeContent $target.Path
    $updated = Remove-LegacyBrandBlock $target.Path $original
    if ($updated -cne $original) {
        [pscustomobject]@{ Path = $target.Path; Content = $updated }
    }
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
