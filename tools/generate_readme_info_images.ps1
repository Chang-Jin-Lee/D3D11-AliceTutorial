[CmdletBinding()]
param(
    [string]$Manifest = 'tools/readme_media_manifest.json',
    [string]$ProjectNumber,
    [switch]$All
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'readme_media_common.ps1')

Add-Type -AssemblyName System.Drawing

function Get-InfoImageProjectSelection {
    param(
        [Parameter(Mandatory)] [object]$Manifest,
        [string]$ProjectNumber,
        [switch]$All
    )

    if ($All) {
        if (-not [string]::IsNullOrWhiteSpace($ProjectNumber)) {
            throw 'Specify either -All or -ProjectNumber, not both.'
        }
        return @($Manifest.projects)
    }

    if ([string]::IsNullOrWhiteSpace($ProjectNumber)) {
        throw 'Specify -All to generate every manifest project or -ProjectNumber to generate one project.'
    }

    $normalizedProjectNumber = if ($ProjectNumber -match '^\d+$') { ([int]$ProjectNumber).ToString('00') } else { $ProjectNumber }
    $selectedProjects = @($Manifest.projects | Where-Object { $_.number -eq $normalizedProjectNumber })
    if ($selectedProjects.Count -eq 0) {
        throw "Project number not found in manifest: $ProjectNumber"
    }

    return $selectedProjects
}

function Resolve-ManifestMediaPath {
    param(
        [Parameter(Mandatory)] [string]$MediaDir,
        [Parameter(Mandatory)] [string]$ManifestPath,
        [Parameter(Mandatory)] [string]$PropertyName
    )

    if ([string]::IsNullOrWhiteSpace($ManifestPath) -or [System.IO.Path]::IsPathRooted($ManifestPath)) {
        throw "Manifest media path must be relative for ${PropertyName}: $ManifestPath"
    }

    $fullMediaDir = [System.IO.Path]::GetFullPath($MediaDir)
    $mediaPrefix = $fullMediaDir.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $resolvedPath = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($fullMediaDir, $ManifestPath))
    if (-not $resolvedPath.StartsWith($mediaPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Manifest media path must stay under mediaDir for ${PropertyName}: $ManifestPath"
    }

    return $resolvedPath
}

function Get-ReadmeInfoFontFamily {
    $installedFonts = $null
    $fontFamilies = @()
    try {
        $installedFonts = [System.Drawing.Text.InstalledFontCollection]::new()
        $fontFamilies = @($installedFonts.Families)
        $fontNames = @($fontFamilies | ForEach-Object { $_.Name })
        foreach ($candidate in @('Malgun Gothic', 'Segoe UI')) {
            if ($fontNames -contains $candidate) {
                return $candidate
            }
        }
        return [System.Drawing.FontFamily]::GenericSansSerif.Name
    }
    finally {
        foreach ($fontFamily in $fontFamilies) {
            if ($null -ne $fontFamily) { $fontFamily.Dispose() }
        }
        if ($null -ne $installedFonts) { $installedFonts.Dispose() }
    }
}

function Get-AspectFitRectangle {
    param(
        [Parameter(Mandatory)] [System.Drawing.Image]$Image,
        [Parameter(Mandatory)] [System.Drawing.Rectangle]$Bounds
    )

    $scale = [Math]::Min($Bounds.Width / [double]$Image.Width, $Bounds.Height / [double]$Image.Height)
    $width = [Math]::Max(1, [int][Math]::Round($Image.Width * $scale))
    $height = [Math]::Max(1, [int][Math]::Round($Image.Height * $scale))
    $x = $Bounds.X + [int][Math]::Floor(($Bounds.Width - $width) / 2.0)
    $y = $Bounds.Y + [int][Math]::Floor(($Bounds.Height - $height) / 2.0)
    return [System.Drawing.Rectangle]::new($x, $y, $width, $height)
}

function New-FittedFont {
    param(
        [Parameter(Mandatory)] [System.Drawing.Graphics]$Graphics,
        [Parameter(Mandatory)] [string]$Text,
        [Parameter(Mandatory)] [string]$Family,
        [Parameter(Mandatory)] [System.Drawing.FontStyle]$Style,
        [Parameter(Mandatory)] [int]$MaximumSize,
        [Parameter(Mandatory)] [int]$MinimumSize,
        [Parameter(Mandatory)] [int]$Width,
        [Parameter(Mandatory)] [int]$Height,
        [Parameter(Mandatory)] [System.Drawing.StringFormat]$Format
    )

    for ($size = $MaximumSize; $size -ge $MinimumSize; $size--) {
        $font = $null
        try {
            $font = [System.Drawing.Font]::new($Family, [single]$size, $Style, [System.Drawing.GraphicsUnit]::Pixel)
            $measured = $Graphics.MeasureString($Text, $font, [System.Drawing.SizeF]::new($Width, 10000), $Format)
            if ($measured.Height -le $Height) {
                $result = $font
                $font = $null
                return $result
            }
        }
        finally {
            if ($null -ne $font) { $font.Dispose() }
        }
    }

    return [System.Drawing.Font]::new($Family, [single]$MinimumSize, $Style, [System.Drawing.GraphicsUnit]::Pixel)
}

function New-ReadmeInfoTagLayout {
    param(
        [Parameter(Mandatory)] [System.Drawing.Graphics]$Graphics,
        [Parameter(Mandatory)] [string[]]$Tags,
        [Parameter(Mandatory)] [string]$FontFamily,
        [Parameter(Mandatory)] [System.Drawing.Rectangle]$Bounds
    )

    $format = $null
    try {
        $format = [System.Drawing.StringFormat]::new()
        $format.Trimming = [System.Drawing.StringTrimming]::None
        foreach ($size in 17..11) {
            $font = $null
            $keepFont = $false
            try {
                $font = [System.Drawing.Font]::new($FontFamily, [single]$size, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
                $pillHeight = [int][Math]::Ceiling($font.GetHeight($Graphics)) + 10
                $items = [System.Collections.Generic.List[object]]::new()
                $x = $Bounds.X
                $y = $Bounds.Y
                $fits = $true
                foreach ($tag in $Tags) {
                    $measured = $Graphics.MeasureString([string]$tag, $font, [System.Drawing.SizeF]::new(10000, 10000), $format)
                    $pillWidth = [int][Math]::Ceiling($measured.Width) + 22
                    if ($pillWidth -gt $Bounds.Width) {
                        $fits = $false
                        break
                    }
                    if ($x + $pillWidth -gt $Bounds.Right -and $x -ne $Bounds.X) {
                        $x = $Bounds.X
                        $y += $pillHeight + 4
                    }
                    if ($y + $pillHeight -gt $Bounds.Bottom) {
                        $fits = $false
                        break
                    }
                    $items.Add([pscustomobject]@{
                        Text = [string]$tag
                        Bounds = [System.Drawing.Rectangle]::new($x, $y, $pillWidth, $pillHeight)
                    })
                    $x += $pillWidth + 4
                }
                if ($fits -and $items.Count -eq $Tags.Count) {
                    $keepFont = $true
                    return [pscustomobject]@{ Font = $font; Items = $items.ToArray() }
                }
            }
            finally {
                if (-not $keepFont -and $null -ne $font) { $font.Dispose() }
            }
        }
        throw "Tags do not fit inside the information image panel for project metadata."
    }
    finally {
        if ($null -ne $format) { $format.Dispose() }
    }
}

function Draw-ReadmeInfoTags {
    param(
        [Parameter(Mandatory)] [System.Drawing.Graphics]$Graphics,
        [Parameter(Mandatory)] [string[]]$Tags,
        [Parameter(Mandatory)] [string]$FontFamily,
        [Parameter(Mandatory)] [System.Drawing.Rectangle]$Bounds
    )

    $layout = $null
    $textBrush = $null
    $fillBrush = $null
    $borderPen = $null
    try {
        $layout = New-ReadmeInfoTagLayout -Graphics $Graphics -Tags $Tags -FontFamily $FontFamily -Bounds $Bounds
        $textBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(236, 242, 248))
        $fillBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(46, 70, 82))
        $borderPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(92, 150, 170), 1)
        foreach ($item in $layout.Items) {
            $Graphics.FillRectangle($fillBrush, $item.Bounds)
            $Graphics.DrawRectangle($borderPen, $item.Bounds)
            $Graphics.DrawString($item.Text, $layout.Font, $textBrush, [System.Drawing.RectangleF]::new($item.Bounds.X + 11, $item.Bounds.Y + 5, $item.Bounds.Width - 22, $item.Bounds.Height - 10))
        }
    }
    finally {
        if ($null -ne $borderPen) { $borderPen.Dispose() }
        if ($null -ne $fillBrush) { $fillBrush.Dispose() }
        if ($null -ne $textBrush) { $textBrush.Dispose() }
        if ($null -ne $layout -and $null -ne $layout.Font) { $layout.Font.Dispose() }
    }
}

function New-ReadmeInfoImage {
    param(
        [Parameter(Mandatory)] [object]$Project,
        [Parameter(Mandatory)] [string]$SourcePath,
        [Parameter(Mandatory)] [string]$OutputPath,
        [Parameter(Mandatory)] [int]$Width,
        [Parameter(Mandatory)] [int]$Height,
        [Parameter(Mandatory)] [string]$FontFamily
    )

    $sourceImage = $null
    $bitmap = $null
    $graphics = $null
    $titleFont = $null
    $summaryFont = $null
    $numberFont = $null
    $textBrush = $null
    $mutedBrush = $null
    $panelBrush = $null
    $screenBrush = $null
    $screenPen = $null
    $format = $null
    try {
        $sourceImage = [System.Drawing.Image]::FromFile($SourcePath)
        $bitmap = [System.Drawing.Bitmap]::new($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([System.Drawing.Color]::FromArgb(29, 34, 40))

        $screenshotBounds = [System.Drawing.Rectangle]::new(40, 40, 940, 560)
        $summaryBounds = [System.Drawing.Rectangle]::new(1030, 64, 520, 512)
        $panelBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(37, 45, 53))
        $screenBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(16, 20, 24))
        $screenPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(89, 112, 126), 1)
        $graphics.FillRectangle($screenBrush, $screenshotBounds)
        $graphics.FillRectangle($panelBrush, $summaryBounds)
        $graphics.DrawRectangle($screenPen, $screenshotBounds)
        $graphics.DrawRectangle($screenPen, $summaryBounds)
        $fittedScreenshotBounds = Get-AspectFitRectangle -Image $sourceImage -Bounds $screenshotBounds
        $graphics.DrawImage($sourceImage, $fittedScreenshotBounds)

        $format = [System.Drawing.StringFormat]::new()
        $format.Trimming = [System.Drawing.StringTrimming]::EllipsisWord
        $format.FormatFlags = [System.Drawing.StringFormatFlags]::LineLimit
        $textBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(247, 249, 251))
        $mutedBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(188, 206, 217))
        $numberFont = [System.Drawing.Font]::new($FontFamily, 20, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
        $graphics.DrawString(('PROJECT {0}' -f $Project.number), $numberFont, $mutedBrush, [System.Drawing.RectangleF]::new(1062, 88, 460, 30), $format)

        $titleFont = New-FittedFont -Graphics $graphics -Text ([string]$Project.title) -Family $FontFamily -Style ([System.Drawing.FontStyle]::Bold) -MaximumSize 46 -MinimumSize 32 -Width 460 -Height 130 -Format $format
        $graphics.DrawString([string]$Project.title, $titleFont, $textBrush, [System.Drawing.RectangleF]::new(1062, 132, 460, 130), $format)

        $summaryFont = New-FittedFont -Graphics $graphics -Text ([string]$Project.summary) -Family $FontFamily -Style ([System.Drawing.FontStyle]::Regular) -MaximumSize 25 -MinimumSize 18 -Width 460 -Height 106 -Format $format
        $graphics.DrawString([string]$Project.summary, $summaryFont, $mutedBrush, [System.Drawing.RectangleF]::new(1062, 278, 460, 106), $format)
        Draw-ReadmeInfoTags -Graphics $graphics -Tags @($Project.tags) -FontFamily $FontFamily -Bounds ([System.Drawing.Rectangle]::new(1062, 414, 460, 144))

        $outputDirectory = Split-Path -Parent $OutputPath
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
        $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($null -ne $format) { $format.Dispose() }
        if ($null -ne $screenPen) { $screenPen.Dispose() }
        if ($null -ne $screenBrush) { $screenBrush.Dispose() }
        if ($null -ne $panelBrush) { $panelBrush.Dispose() }
        if ($null -ne $mutedBrush) { $mutedBrush.Dispose() }
        if ($null -ne $textBrush) { $textBrush.Dispose() }
        if ($null -ne $numberFont) { $numberFont.Dispose() }
        if ($null -ne $summaryFont) { $summaryFont.Dispose() }
        if ($null -ne $titleFont) { $titleFont.Dispose() }
        if ($null -ne $graphics) { $graphics.Dispose() }
        if ($null -ne $bitmap) { $bitmap.Dispose() }
        if ($null -ne $sourceImage) { $sourceImage.Dispose() }
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$manifestData = Get-ReadmeMediaManifest -ManifestPath $Manifest -RepoRoot $repoRoot
$manifestErrors = @(Test-ReadmeMediaManifest -Manifest $manifestData -RepoRoot $repoRoot)
if ($manifestErrors.Count -gt 0) {
    throw "README media manifest validation failed: $($manifestErrors -join '; ')"
}
if ([int]$manifestData.infoWidth -ne 1600 -or [int]$manifestData.infoHeight -ne 640) {
    throw 'README information image dimensions must be 1600x640.'
}

$mediaDir = Resolve-ReadmeMediaPath -RepoRoot $repoRoot -Path ([string]$manifestData.mediaDir)
$fontFamily = Get-ReadmeInfoFontFamily
$selectedProjects = @(Get-InfoImageProjectSelection -Manifest $manifestData -ProjectNumber $ProjectNumber -All:$All)
foreach ($project in $selectedProjects) {
    $sourcePath = Resolve-ManifestMediaPath -MediaDir $mediaDir -ManifestPath ([string]$project.image) -PropertyName 'project.image'
    $outputPath = Resolve-ManifestMediaPath -MediaDir $mediaDir -ManifestPath ([string]$project.infoImage) -PropertyName 'project.infoImage'
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Screenshot not found for project $($project.number): $sourcePath"
    }
    New-ReadmeInfoImage -Project $project -SourcePath $sourcePath -OutputPath $outputPath -Width ([int]$manifestData.infoWidth) -Height ([int]$manifestData.infoHeight) -FontFamily $fontFamily
    Write-Host "Generated information image for project $($project.number): $outputPath"
}
