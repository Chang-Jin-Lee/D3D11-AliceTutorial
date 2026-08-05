param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Manifest = 'tools/readme_media_manifest.json'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = [IO.Path]::GetFullPath($RepoRoot)
$manifestPath = if ([IO.Path]::IsPathRooted($Manifest)) { $Manifest } else { Join-Path $root $Manifest }
$data = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
if (@($data.projects).Count -ne [int]$data.expectedProjectCount) { throw 'manifest project count mismatch' }

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
    $content = (Read-ReadmeContent $Path).Replace("`r`n", "`n")
    $newline = "`n"
    $start = '<!-- README-BRAND:START -->'
    $end = '<!-- README-BRAND:END -->'
    $startCount = ([regex]::Matches($content, [regex]::Escape($start))).Count
    $endCount = ([regex]::Matches($content, [regex]::Escape($end))).Count
    if ($startCount -ne $endCount -or $startCount -gt 1) { throw "malformed README-BRAND markers: $Path" }
    $block = New-BrandBlock $ImagePath $Width $newline
    if ($startCount -eq 1) {
        $pattern = [regex]::Escape($start) + '.*?' + [regex]::Escape($end)
        $updated = [regex]::Replace($content, $pattern, [Text.RegularExpressions.MatchEvaluator]{ param($match) $block }, [Text.RegularExpressions.RegexOptions]::Singleline)
        return [regex]::Replace($updated, '(?m)(^#{1,6}\s+.+?)\r\r\n(?=<!-- README-BRAND:START -->)', "`$1`r`n")
    }
    $heading = [regex]::Match($content, '(?m)^#{1,6}\s+.+?(?=\r?$)')
    if (-not $heading.Success) { throw "first Markdown heading missing: $Path" }
    $insertAt = $heading.Index + $heading.Length
    return $content.Insert($insertAt, $newline + $newline + $block)
}

$targets = [Collections.Generic.List[object]]::new()
$targets.Add([pscustomobject]@{ Path = Join-Path $root 'README.md'; Image = 'docs/media/branding/alice-tutorial-logo.png'; Width = 720 })
foreach ($project in @($data.projects)) {
    if ([string]$project.directory -notmatch '^\d{2}_[A-Za-z0-9_]+$') { throw "unsafe project directory: $($project.directory)" }
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
