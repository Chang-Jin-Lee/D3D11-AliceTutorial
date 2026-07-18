[CmdletBinding()]
param(
    [string]$Manifest = 'tools/readme_media_manifest.json',
    [string]$OutputDir = (Join-Path $env:TEMP 'D3D11-AliceTutorial-readme-review')
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'readme_media_common.ps1')

Add-Type -AssemblyName System.Drawing

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

function Get-ReviewAspectFitRectangle {
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

function Select-GifMidpointFrame {
    param([Parameter(Mandatory)] [System.Drawing.Image]$Image)

    $frameDimension = [System.Drawing.Imaging.FrameDimension]::new([Guid]$Image.FrameDimensionsList[0])
    $frameCount = $Image.GetFrameCount($frameDimension)
    if ($frameCount -lt 1) { throw 'GIF contains no frames.' }
    $null = $Image.SelectActiveFrame($frameDimension, [int][Math]::Floor($frameCount / 2.0))
}

function New-ReadmeReviewSheet {
    param(
        [Parameter(Mandatory)] [object[]]$Projects,
        [Parameter(Mandatory)] [string]$MediaDir,
        [Parameter(Mandatory)] [string]$Kind,
        [Parameter(Mandatory)] [string]$OutputPath
    )

    $columns = 5
    $margin = 16
    $cellWidth = 300
    $cellHeight = 210
    $rows = [int][Math]::Ceiling($Projects.Count / [double]$columns)
    $canvasWidth = ($columns * $cellWidth) + (($columns + 1) * $margin)
    $canvasHeight = ($rows * $cellHeight) + (($rows + 1) * $margin)
    $bitmap = $null
    $graphics = $null
    $numberFont = $null
    $numberBrush = $null
    $cellBrush = $null
    $thumbnailBrush = $null
    $borderPen = $null
    $format = $null
    try {
        $bitmap = [System.Drawing.Bitmap]::new($canvasWidth, $canvasHeight, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.Clear([System.Drawing.Color]::FromArgb(29, 34, 40))
        $numberFont = [System.Drawing.Font]::new('Segoe UI', 20, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
        $numberBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(242, 246, 249))
        $cellBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(37, 45, 53))
        $thumbnailBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(16, 20, 24))
        $borderPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(85, 111, 127), 1)
        $format = [System.Drawing.StringFormat]::new()
        $format.Alignment = [System.Drawing.StringAlignment]::Center
        $format.LineAlignment = [System.Drawing.StringAlignment]::Center

        for ($index = 0; $index -lt $Projects.Count; $index++) {
            $project = $Projects[$index]
            $column = $index % $columns
            $row = [int][Math]::Floor($index / [double]$columns)
            $cellBounds = [System.Drawing.Rectangle]::new($margin + ($column * ($cellWidth + $margin)), $margin + ($row * ($cellHeight + $margin)), $cellWidth, $cellHeight)
            $numberBounds = [System.Drawing.Rectangle]::new($cellBounds.X, $cellBounds.Y + 8, $cellBounds.Width, 28)
            $thumbnailBounds = [System.Drawing.Rectangle]::new($cellBounds.X + 12, $cellBounds.Y + 48, $cellBounds.Width - 24, $cellBounds.Height - 60)
            $sourceImage = $null
            try {
                $sourceName = if ($Kind -eq 'gif') { [string]$project.gif } else { [string]$project.image }
                $propertyName = if ($Kind -eq 'gif') { 'project.gif' } else { 'project.image' }
                $sourcePath = Resolve-ManifestMediaPath -MediaDir $MediaDir -ManifestPath $sourceName -PropertyName $propertyName
                if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
                    throw "Missing $Kind source for project $($project.number): $sourcePath"
                }
                $sourceImage = [System.Drawing.Image]::FromFile($sourcePath)
                if ($Kind -eq 'gif') { Select-GifMidpointFrame -Image $sourceImage }
                $graphics.FillRectangle($cellBrush, $cellBounds)
                $graphics.FillRectangle($thumbnailBrush, $thumbnailBounds)
                $graphics.DrawRectangle($borderPen, $cellBounds)
                $graphics.DrawRectangle($borderPen, $thumbnailBounds)
                $graphics.DrawString([string]$project.number, $numberFont, $numberBrush, [System.Drawing.RectangleF]::new($numberBounds.X, $numberBounds.Y, $numberBounds.Width, $numberBounds.Height), $format)
                $graphics.DrawImage($sourceImage, (Get-ReviewAspectFitRectangle -Image $sourceImage -Bounds $thumbnailBounds))
            }
            finally {
                if ($null -ne $sourceImage) { $sourceImage.Dispose() }
            }
        }

        $outputDirectory = Split-Path -Parent $OutputPath
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
        $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($null -ne $format) { $format.Dispose() }
        if ($null -ne $borderPen) { $borderPen.Dispose() }
        if ($null -ne $thumbnailBrush) { $thumbnailBrush.Dispose() }
        if ($null -ne $cellBrush) { $cellBrush.Dispose() }
        if ($null -ne $numberBrush) { $numberBrush.Dispose() }
        if ($null -ne $numberFont) { $numberFont.Dispose() }
        if ($null -ne $graphics) { $graphics.Dispose() }
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$manifestData = Get-ReadmeMediaManifest -ManifestPath $Manifest -RepoRoot $repoRoot
$manifestErrors = @(Test-ReadmeMediaManifest -Manifest $manifestData -RepoRoot $repoRoot)
if ($manifestErrors.Count -gt 0) {
    throw "README media manifest validation failed: $($manifestErrors -join '; ')"
}

$mediaDir = Resolve-ReadmeMediaPath -RepoRoot $repoRoot -Path ([string]$manifestData.mediaDir)
$resolvedOutputDir = Resolve-ReadmeMediaPath -RepoRoot $repoRoot -Path $OutputDir
New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null
New-ReadmeReviewSheet -Projects @($manifestData.projects) -MediaDir $mediaDir -Kind 'png' -OutputPath (Join-Path $resolvedOutputDir 'readme-png-review.png')
New-ReadmeReviewSheet -Projects @($manifestData.projects) -MediaDir $mediaDir -Kind 'gif' -OutputPath (Join-Path $resolvedOutputDir 'readme-gif-review.png')
Write-Host "Generated review sheets in $resolvedOutputDir"
