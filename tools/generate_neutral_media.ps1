param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

function New-Image([string]$Path, [string]$Title, [string]$Subtitle, [System.Drawing.Color]$BackColor) {
    $dir = Split-Path -Parent $Path
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $bmp = $null
    $g = $null
    $fontTitle = $null
    $fontSub = $null
    $brush = $null
    try {
        $bmp = New-Object System.Drawing.Bitmap 1280, 720
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.Clear($BackColor)
        $fontTitle = New-Object System.Drawing.Font 'Segoe UI', 56, ([System.Drawing.FontStyle]::Bold)
        $fontSub = New-Object System.Drawing.Font 'Segoe UI', 28, ([System.Drawing.FontStyle]::Regular)
        $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
        $g.DrawString($Title, $fontTitle, $brush, 80, 240)
        $g.DrawString($Subtitle, $fontSub, $brush, 86, 330)
        $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($null -ne $brush) { $brush.Dispose() }
        if ($null -ne $fontSub) { $fontSub.Dispose() }
        if ($null -ne $fontTitle) { $fontTitle.Dispose() }
        if ($null -ne $g) { $g.Dispose() }
        if ($null -ne $bmp) { $bmp.Dispose() }
    }
}

function New-Wav([string]$Path, [int]$FrequencyHz, [double]$Seconds, [double]$Volume) {
    $dir = Split-Path -Parent $Path
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $sampleRate = 44100
    $samples = [int]($sampleRate * $Seconds)
    $dataBytes = $samples * 2
    $fs = $null
    $bw = $null
    try {
        $fs = [System.IO.File]::Create($Path)
        $bw = New-Object System.IO.BinaryWriter $fs
        $bw.Write([Text.Encoding]::ASCII.GetBytes('RIFF'))
        $bw.Write([int](36 + $dataBytes))
        $bw.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
        $bw.Write([int]16)
        $bw.Write([int16]1)
        $bw.Write([int16]1)
        $bw.Write([int]$sampleRate)
        $bw.Write([int]($sampleRate * 2))
        $bw.Write([int16]2)
        $bw.Write([int16]16)
        $bw.Write([Text.Encoding]::ASCII.GetBytes('data'))
        $bw.Write([int]$dataBytes)
        for ($i = 0; $i -lt $samples; $i++) {
            $t = $i / $sampleRate
            $env = [Math]::Min(1.0, $i / 500.0) * [Math]::Min(1.0, ($samples - $i) / 1000.0)
            $v = [Math]::Sin(2.0 * [Math]::PI * $FrequencyHz * $t) * $Volume * $env
            $bw.Write([int16]([Math]::Round($v * 32767)))
        }
    }
    finally {
        if ($null -ne $bw) { $bw.Dispose() }
        if ($null -ne $fs) { $fs.Dispose() }
    }
}

$imageRoot = Join-Path $RepoRoot 'Dx11\Resource\Image\Public'
$soundRoot = Join-Path $RepoRoot 'Dx11\Resource\Sound\Public'

New-Image (Join-Path $imageRoot 'Loading.png') 'MyAlice D3D11' 'Loading public demo assets' ([System.Drawing.Color]::FromArgb(26, 32, 44))
New-Image (Join-Path $imageRoot 'LoadingDone.png') 'Ready' 'Click to start the public asset demo' ([System.Drawing.Color]::FromArgb(22, 64, 58))
New-Image (Join-Path $imageRoot 'Comic\01.png') 'Scene 01' 'Character asset replacement sample' ([System.Drawing.Color]::FromArgb(48, 52, 70))
New-Image (Join-Path $imageRoot 'Comic\02.png') 'Scene 02' 'External animation clip test' ([System.Drawing.Color]::FromArgb(54, 48, 70))
New-Image (Join-Path $imageRoot 'Comic\03.png') 'Scene 03' 'Neutral media path complete' ([System.Drawing.Color]::FromArgb(70, 55, 42))

New-Wav (Join-Path $soundRoot 'ui_advance.wav') 660 0.15 0.25
New-Wav (Join-Path $soundRoot 'ui_done.wav') 880 0.25 0.22
New-Wav (Join-Path $soundRoot 'step.wav') 220 0.08 0.18
New-Wav (Join-Path $soundRoot 'run.wav') 330 0.08 0.18
New-Wav (Join-Path $soundRoot 'action.wav') 740 0.12 0.20
New-Wav (Join-Path $soundRoot 'reload.wav') 520 0.18 0.20

Write-Host '[done] generated neutral media'
