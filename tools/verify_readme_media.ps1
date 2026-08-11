[CmdletBinding()]
param(
    [string]$RepoRoot,
    [string]$Manifest
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'readme_media_common.ps1')
Add-Type -AssemblyName System.Drawing

function Add-VerificationError {
    param(
        [System.Collections.Generic.List[string]]$Errors,
        [string]$Message
    )

    $null = $Errors.Add($Message)
}

function Test-NoExistingReparsePoint {
    param(
        [string]$ContainmentRoot,
        [string]$TargetPath,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Errors
    )

    try {
        $rootFullPath = [System.IO.Path]::GetFullPath($ContainmentRoot)
        $targetFullPath = [System.IO.Path]::GetFullPath($TargetPath)
        $relative = [System.IO.Path]::GetRelativePath($rootFullPath, $targetFullPath)
        if ([System.IO.Path]::IsPathRooted($relative) -or $relative -match '^\.\.([\\/]|$)') {
            Add-VerificationError $Errors "$Label must be contained in $rootFullPath"
            return $false
        }

        $pathsToInspect = [System.Collections.Generic.List[string]]::new()
        $pathsToInspect.Add($rootFullPath)
        $currentPath = $rootFullPath
        if ($relative -ne '.') {
            foreach ($segment in @($relative -split '[\\/]')) {
                $currentPath = Join-Path $currentPath $segment
                $pathsToInspect.Add($currentPath)
            }
        }

        foreach ($pathToInspect in $pathsToInspect) {
            try {
                $item = Get-Item -LiteralPath $pathToInspect -Force -ErrorAction Stop
            }
            catch [System.Management.Automation.ItemNotFoundException] {
                continue
            }
            if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                Add-VerificationError $Errors "$Label contains a reparse point: $pathToInspect"
                return $false
            }
        }

        return $true
    }
    catch {
        Add-VerificationError $Errors "$Label canonical containment check failed: $($_.Exception.Message)"
        return $false
    }
}

function Resolve-ContainedRelativePath {
    param(
        [string]$BasePath,
        [string]$RelativePath,
        [string]$ContainmentRoot,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Errors
    )

    if ([string]::IsNullOrWhiteSpace($RelativePath) -or [System.IO.Path]::IsPathRooted($RelativePath)) {
        Add-VerificationError $Errors "$Label must be a contained relative path"
        return $null
    }

    try {
        $baseFullPath = [System.IO.Path]::GetFullPath($BasePath)
        $candidate = [System.IO.Path]::GetFullPath((Join-Path $baseFullPath $RelativePath))
        $relative = [System.IO.Path]::GetRelativePath($baseFullPath, $candidate)
        if ([System.IO.Path]::IsPathRooted($relative) -or $relative -match '^\.\.([\\/]|$)') {
            Add-VerificationError $Errors "$Label must be contained in $baseFullPath"
            return $null
        }

        if (-not (Test-NoExistingReparsePoint -ContainmentRoot $ContainmentRoot -TargetPath $candidate -Label $Label -Errors $Errors)) {
            return $null
        }

        return $candidate
    }
    catch {
        Add-VerificationError $Errors "$Label must be a contained relative path: $($_.Exception.Message)"
        return $null
    }
}

function Test-BytePrefix {
    param([string]$Path, [byte[]]$Expected)

    $stream = $null
    try {
        $stream = [System.IO.File]::OpenRead($Path)
        if ($stream.Length -lt $Expected.Length) { return $false }
        foreach ($expectedByte in $Expected) {
            if ($stream.ReadByte() -ne $expectedByte) { return $false }
        }
        return $true
    }
    finally {
        if ($null -ne $stream) { $stream.Dispose() }
    }
}

function Get-SampledRgb {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [int]$Columns = 32,
        [int]$Rows = 18
    )

    $samples = [int[]]::new($Columns * $Rows)
    $sampleIndex = 0
    for ($row = 0; $row -lt $Rows; $row++) {
        $y = [math]::Min($Bitmap.Height - 1, [int][math]::Floor((($row + 0.5) * $Bitmap.Height) / $Rows))
        for ($column = 0; $column -lt $Columns; $column++) {
            $x = [math]::Min($Bitmap.Width - 1, [int][math]::Floor((($column + 0.5) * $Bitmap.Width) / $Columns))
            $color = $Bitmap.GetPixel($x, $y)
            $inverseAlpha = 255 - $color.A
            $red = [int][math]::Round((($color.R * $color.A) + (255 * $inverseAlpha)) / 255.0)
            $green = [int][math]::Round((($color.G * $color.A) + (255 * $inverseAlpha)) / 255.0)
            $blue = [int][math]::Round((($color.B * $color.A) + (255 * $inverseAlpha)) / 255.0)
            $samples[$sampleIndex++] = ($red -shl 16) -bor ($green -shl 8) -bor $blue
        }
    }

    return $samples
}

function Get-LuminanceVariance {
    param([int[]]$RgbSamples)

    if ($RgbSamples.Count -eq 0) { return 0.0 }
    $luminances = [double[]]::new($RgbSamples.Count)
    $sum = 0.0
    for ($index = 0; $index -lt $RgbSamples.Count; $index++) {
        $rgb = $RgbSamples[$index]
        $red = ($rgb -shr 16) -band 0xFF
        $green = ($rgb -shr 8) -band 0xFF
        $blue = $rgb -band 0xFF
        $luminance = (0.2126 * $red) + (0.7152 * $green) + (0.0722 * $blue)
        $luminances[$index] = $luminance
        $sum += $luminance
    }

    $mean = $sum / $RgbSamples.Count
    $squaredDifference = 0.0
    foreach ($luminance in $luminances) {
        $difference = $luminance - $mean
        $squaredDifference += $difference * $difference
    }
    return $squaredDifference / $RgbSamples.Count
}

function Test-PngMedia {
    param(
        [string]$Path,
        [int]$ExpectedWidth,
        [int]$ExpectedHeight,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Errors
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Add-VerificationError $Errors "$Label missing: $Path"
        return
    }
    if ((Get-Item -LiteralPath $Path).Length -eq 0) {
        Add-VerificationError $Errors "$Label is empty: $Path"
        return
    }

    $pngSignature = [byte[]](0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A)
    if (-not (Test-BytePrefix -Path $Path -Expected $pngSignature)) {
        Add-VerificationError $Errors "$Label has an invalid PNG signature: $Path"
        return
    }

    $bitmap = $null
    try {
        $bitmap = [System.Drawing.Bitmap]::new($Path)
        if ($bitmap.Width -ne $ExpectedWidth -or $bitmap.Height -ne $ExpectedHeight) {
            Add-VerificationError $Errors "$Label dimensions are $($bitmap.Width)x$($bitmap.Height); expected ${ExpectedWidth}x${ExpectedHeight}: $Path"
        }

        $samples = @(Get-SampledRgb -Bitmap $bitmap)
        $uniqueColors = [System.Collections.Generic.HashSet[int]]::new()
        foreach ($sample in $samples) { $null = $uniqueColors.Add($sample) }
        if ($uniqueColors.Count -lt 8) {
            Add-VerificationError $Errors "$Label sampled color count is $($uniqueColors.Count); expected at least 8: $Path"
        }

        $variance = Get-LuminanceVariance -RgbSamples $samples
        if ($variance -le 4.0) {
            Add-VerificationError $Errors ("$Label luminance variance is {0:F3}; expected above 4.0: $Path" -f $variance)
        }
    }
    catch {
        Add-VerificationError $Errors "$Label decode failed: $Path ($($_.Exception.Message))"
    }
    finally {
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

function Test-GifMedia {
    param(
        [string]$Path,
        [string]$Label,
        [int]$ExpectedFps,
        [double]$ExpectedSeconds,
        [int64]$ExpectedMaxBytes,
        [System.Collections.Generic.List[string]]$Errors
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Add-VerificationError $Errors "$Label missing: $Path"
        return
    }

    $fileLength = (Get-Item -LiteralPath $Path).Length
    if ($fileLength -eq 0) {
        Add-VerificationError $Errors "$Label is empty: $Path"
        return
    }
    if ($fileLength -gt $ExpectedMaxBytes) {
        Add-VerificationError $Errors "$Label size is $fileLength bytes; expected at most $ExpectedMaxBytes bytes: $Path"
    }

    $isGif87a = Test-BytePrefix -Path $Path -Expected ([System.Text.Encoding]::ASCII.GetBytes('GIF87a'))
    $isGif89a = Test-BytePrefix -Path $Path -Expected ([System.Text.Encoding]::ASCII.GetBytes('GIF89a'))
    if (-not $isGif87a -and -not $isGif89a) {
        Add-VerificationError $Errors "$Label has an invalid GIF signature: $Path"
        return
    }

    $image = $null
    try {
        $image = [System.Drawing.Image]::FromFile($Path)
        if ($image.Width -ne 800 -or $image.Height -ne 450) {
            Add-VerificationError $Errors "$Label dimensions are $($image.Width)x$($image.Height); expected 800x450: $Path"
        }

        $frameDimension = [System.Drawing.Imaging.FrameDimension]::new($image.FrameDimensionsList[0])
        $frameCount = $image.GetFrameCount($frameDimension)
        if ($frameCount -lt 2) {
            Add-VerificationError $Errors "$Label frame count is $frameCount; expected at least 2: $Path"
        }
        $totalDelaySeconds = $null
        try {
            $delayBytes = $image.GetPropertyItem(0x5100).Value
            if ($delayBytes.Length -lt ($frameCount * 4)) {
                Add-VerificationError $Errors "$Label decoded delay table is incomplete: $Path"
            }
            else {
                $totalDelayHundredths = [uint64]0
                for ($index = 0; $index -lt $frameCount; $index++) {
                    $totalDelayHundredths += [System.BitConverter]::ToUInt32($delayBytes, $index * 4)
                }
                $totalDelaySeconds = $totalDelayHundredths / 100.0
                $minExpectedSeconds = $ExpectedSeconds - 0.5
                $maxExpectedSeconds = $ExpectedSeconds + 0.5
                if ([math]::Abs($totalDelaySeconds - $ExpectedSeconds) -gt 0.5) {
                    Add-VerificationError $Errors ("$Label total decoded delay is {0:F2}s; expected {1:F1}-{2:F1}s: $Path" -f $totalDelaySeconds, $minExpectedSeconds, $maxExpectedSeconds)
                }
                $decodedFps = $frameCount / $totalDelaySeconds
                if ([math]::Abs($decodedFps - $ExpectedFps) -gt 0.1) {
                    Add-VerificationError $Errors ("$Label frame cadence is {0:F2}fps; expected {1:F2}fps: $Path" -f $decodedFps, $ExpectedFps)
                }
            }
        }
        catch {
            Add-VerificationError $Errors "$Label decoded delay is unavailable: $Path"
        }

        $null = $image.SelectActiveFrame($frameDimension, 0)
        $firstSamples = @(Get-SampledRgb -Bitmap ([System.Drawing.Bitmap]$image))

        $maximumChangedRatio = 0.0
        for ($frameIndex = 1; $frameIndex -lt $frameCount; $frameIndex++) {
            $null = $image.SelectActiveFrame($frameDimension, $frameIndex)
            $frameSamples = @(Get-SampledRgb -Bitmap ([System.Drawing.Bitmap]$image))
            $changed = 0
            for ($sampleIndex = 0; $sampleIndex -lt $firstSamples.Count; $sampleIndex++) {
                if ($firstSamples[$sampleIndex] -ne $frameSamples[$sampleIndex]) { $changed++ }
            }
            $changedRatio = $changed / [double]$firstSamples.Count
            if ($changedRatio -gt $maximumChangedRatio) { $maximumChangedRatio = $changedRatio }
        }

        if ($maximumChangedRatio -le 0.002) {
            Add-VerificationError $Errors ("$Label sampled motion is {0:F3}%; expected above 0.2%: $Path" -f ($maximumChangedRatio * 100.0))
        }
    }
    catch {
        Add-VerificationError $Errors "$Label decode failed: $Path ($($_.Exception.Message))"
    }
    finally {
        if ($null -ne $image) { $image.Dispose() }
    }
}

function Get-ReadmeText {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $offset = 0
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $encoding = [System.Text.UTF8Encoding]::new($true, $true)
        $offset = 3
    }
    elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
        $encoding = [System.Text.UnicodeEncoding]::new($false, $true, $true)
        $offset = 2
    }
    elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFE -and $bytes[1] -eq 0xFF) {
        $encoding = [System.Text.UnicodeEncoding]::new($true, $true, $true)
        $offset = 2
    }
    else {
        $encoding = [System.Text.UTF8Encoding]::new($false, $true)
    }

    try {
        return $encoding.GetString($bytes, $offset, $bytes.Length - $offset)
    }
    catch [System.Text.DecoderFallbackException] {
        [System.Text.Encoding]::RegisterProvider([System.Text.CodePagesEncodingProvider]::Instance)
        $cp949 = [System.Text.Encoding]::GetEncoding(949, [System.Text.EncoderExceptionFallback]::new(), [System.Text.DecoderExceptionFallback]::new())
        return $cp949.GetString($bytes)
    }
}

function Get-MarkerDetails {
    param([string]$Content, [string]$Name)

    $start = "<!-- $Name`:START -->"
    $end = "<!-- $Name`:END -->"
    $startMatches = [regex]::Matches($Content, [regex]::Escape($start))
    $endMatches = [regex]::Matches($Content, [regex]::Escape($end))
    return [pscustomobject]@{
        Name = $Name
        Start = $start
        End = $end
        StartCount = $startMatches.Count
        EndCount = $endMatches.Count
        StartIndex = $Content.IndexOf($start, [System.StringComparison]::Ordinal)
        EndIndex = $Content.IndexOf($end, [System.StringComparison]::Ordinal)
    }
}

function Get-MarkerBlock {
    param([string]$Content, [object]$Details)

    if ($Details.StartCount -ne 1 -or $Details.EndCount -ne 1 -or $Details.EndIndex -lt $Details.StartIndex) {
        return $null
    }
    $length = $Details.EndIndex + $Details.End.Length - $Details.StartIndex
    return $Content.Substring($Details.StartIndex, $length).Replace("`r`n", "`n")
}

function New-ExpectedNavigationBlock {
    param([string]$Marker, [object[]]$Projects, [int]$Index)

    $previous = if ($Index -eq 0) { '이전' } else { "[이전](../$($Projects[$Index - 1].directory)/README.md)" }
    $next = if ($Index -eq ($Projects.Count - 1)) { '다음' } else { "[다음](../$($Projects[$Index + 1].directory)/README.md)" }
    return @(
        "<!-- $Marker`:START -->"
        '<div align="center">'
        ''
        "$previous | [메인](../../README.md) | [상위](../) | $next"
        ''
        '</div>'
        "<!-- $Marker`:END -->"
    ) -join "`n"
}

function Test-ProjectReadme {
    param(
        [string]$Path,
        [object]$Project,
        [object[]]$Projects,
        [int]$Index,
        [System.Collections.Generic.List[string]]$Errors
    )

    $number = [string]$Project.number
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Add-VerificationError $Errors "Project README missing for $number`: $Path"
        return
    }

    try {
        $content = Get-ReadmeText $Path
    }
    catch {
        Add-VerificationError $Errors "Project README decode failed for $number`: $($_.Exception.Message)"
        return
    }

    $markerNames = @('README-NAV-TOP', 'README-INFO', 'README-RUNTIME', 'README-NAV-BOTTOM')
    $details = @($markerNames | ForEach-Object { Get-MarkerDetails -Content $content -Name $_ })
    foreach ($detail in $details) {
        if ($detail.StartCount -ne 1 -or $detail.EndCount -ne 1) {
            Add-VerificationError $Errors "Project README $number marker $($detail.Name) must have exactly one start/end pair"
        }
        elseif ($detail.EndIndex -lt $detail.StartIndex) {
            Add-VerificationError $Errors "Project README $number marker $($detail.Name) is reversed"
        }
    }

    $completeDetails = @($details | Where-Object { $_.StartCount -eq 1 -and $_.EndCount -eq 1 -and $_.EndIndex -gt $_.StartIndex })
    if ($completeDetails.Count -eq $details.Count) {
        $orderedStarts = @($details.StartIndex)
        for ($orderIndex = 1; $orderIndex -lt $orderedStarts.Count; $orderIndex++) {
            if ($orderedStarts[$orderIndex] -le $orderedStarts[$orderIndex - 1]) {
                Add-VerificationError $Errors "Project README $number generated blocks are not in the required order"
                break
            }
        }

        $expectedBlocks = @{
            'README-NAV-TOP' = New-ExpectedNavigationBlock -Marker 'README-NAV-TOP' -Projects $Projects -Index $Index
            'README-INFO' = @(
                '<!-- README-INFO:START -->'
                ('<p align="center"><img src="../../docs/media/readme/{0}" width="100%" /></p>' -f $Project.infoImage)
                '<!-- README-INFO:END -->'
            ) -join "`n"
            'README-RUNTIME' = @(
                '<!-- README-RUNTIME:START -->'
                '## 실행 화면'
                ''
                '| Screenshot | GIF |'
                '|---|---|'
                ('| <img src="../../docs/media/readme/{0}" width="100%" /> | <img src="../../docs/media/readme/{1}" width="100%" /> |' -f $Project.image, $Project.gif)
                '<!-- README-RUNTIME:END -->'
            ) -join "`n"
            'README-NAV-BOTTOM' = New-ExpectedNavigationBlock -Marker 'README-NAV-BOTTOM' -Projects $Projects -Index $Index
        }

        foreach ($detail in $details) {
            $actualBlock = Get-MarkerBlock -Content $content -Details $detail
            if ($actualBlock -cne $expectedBlocks[$detail.Name]) {
                Add-VerificationError $Errors "Project README $number generated block is incorrect: $($detail.Name)"
            }
        }
    }

    $preservedBody = $content
    foreach ($detail in $details) {
        $pattern = '(?s)' + [regex]::Escape($detail.Start) + '.*?' + [regex]::Escape($detail.End)
        $preservedBody = [regex]::Replace($preservedBody, $pattern, '')
    }
    if ($preservedBody.Trim().Length -lt 50) {
        Add-VerificationError $Errors "Project README $number preserved non-generated body is shorter than 50 characters"
    }
}

function Split-MarkdownRow {
    param([string]$Line)

    $trimmed = $Line.Trim()
    if ($trimmed.StartsWith('|')) { $trimmed = $trimmed.Substring(1) }
    if ($trimmed.EndsWith('|')) {
        $trailingBackslashRun = 0
        for ($index = $trimmed.Length - 2; $index -ge 0 -and $trimmed[$index] -eq '\'; $index--) {
            $trailingBackslashRun++
        }
        if (($trailingBackslashRun % 2) -eq 0) {
            $trimmed = $trimmed.Substring(0, $trimmed.Length - 1)
        }
    }

    $cells = [System.Collections.Generic.List[string]]::new()
    $cell = [System.Text.StringBuilder]::new()
    $backslashRun = 0
    for ($index = 0; $index -lt $trimmed.Length; $index++) {
        $character = $trimmed[$index]
        if ($character -eq '\') {
            $null = $cell.Append($character)
            $backslashRun++
        }
        elseif ($character -eq '|') {
            if (($backslashRun % 2) -eq 1) {
                $cell.Length--
                $null = $cell.Append('|')
            }
            else {
                $cells.Add($cell.ToString().Trim())
                $null = $cell.Clear()
            }
            $backslashRun = 0
        }
        else {
            $null = $cell.Append($character)
            $backslashRun = 0
        }
    }
    $cells.Add($cell.ToString().Trim())
    return $cells.ToArray()
}

function Get-CaptureReportRows {
    param(
        [string]$Content,
        [System.Collections.Generic.List[string]]$Errors
    )

    $lines = @($Content -split "`r?`n")
    $headerIndex = -1
    for ($index = 0; $index -lt $lines.Count; $index++) {
        if ($lines[$index] -match '^\s*\|' -and $lines[$index] -match '\bProject\b' -and $lines[$index] -match '\bStatus\b') {
            $headerIndex = $index
            break
        }
    }
    if ($headerIndex -lt 0) {
        Add-VerificationError $Errors 'capture report is missing a Project/Status table header'
        return @()
    }

    $headers = @(Split-MarkdownRow $lines[$headerIndex])
    foreach ($requiredHeader in @('Project', 'Output', 'Status')) {
        if ($requiredHeader -notin $headers) {
            Add-VerificationError $Errors "capture report is missing the $requiredHeader column"
        }
    }

    $rows = [System.Collections.Generic.List[object]]::new()
    for ($index = $headerIndex + 2; $index -lt $lines.Count; $index++) {
        if ($lines[$index] -notmatch '^\s*\|') { break }
        $cells = @(Split-MarkdownRow $lines[$index])
        if ($cells.Count -ne $headers.Count) {
            Add-VerificationError $Errors "capture report row has $($cells.Count) cells; expected $($headers.Count): $($lines[$index])"
            continue
        }
        $row = [ordered]@{}
        for ($cellIndex = 0; $cellIndex -lt $headers.Count; $cellIndex++) {
            $row[$headers[$cellIndex]] = $cells[$cellIndex]
        }
        $rows.Add([pscustomobject]$row)
    }
    return $rows.ToArray()
}

function Test-CaptureReport {
    param(
        [string]$Path,
        [object[]]$Projects,
        [string]$MediaDir,
        [System.Collections.Generic.List[string]]$Errors
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Add-VerificationError $Errors "capture report missing: $Path"
        return
    }

    try {
        $rows = @(Get-CaptureReportRows -Content (Get-ReadmeText $Path) -Errors $Errors)
    }
    catch {
        Add-VerificationError $Errors "capture report decode failed: $($_.Exception.Message)"
        return
    }

    foreach ($project in $Projects) {
        $number = [string]$project.number
        $projectRows = @($rows | Where-Object { [string]$_.Project -eq $number })
        if ($projectRows.Count -eq 0) {
            Add-VerificationError $Errors "capture report has no final status for project $number"
            continue
        }

        $hasAttemptColumn = $null -ne $projectRows[0].PSObject.Properties['Attempt']
        $maximumAttempt = 0
        if ($hasAttemptColumn) {
            foreach ($row in $projectRows) {
                $parsedAttempt = 0
                if (-not [int]::TryParse([string]$row.Attempt, [ref]$parsedAttempt)) {
                    Add-VerificationError $Errors "capture report has an invalid attempt for project $number`: $($row.Attempt)"
                    $parsedAttempt = -1
                }
                elseif ($parsedAttempt -gt $maximumAttempt) {
                    $maximumAttempt = $parsedAttempt
                }
                $row | Add-Member -NotePropertyName VerificationAttempt -NotePropertyValue $parsedAttempt -Force
            }
            $finalRows = @($projectRows | Where-Object { $_.VerificationAttempt -eq $maximumAttempt })
        }
        else {
            $finalRows = $projectRows
        }

        $nonSuccessRows = @($finalRows | Where-Object { [string]$_.Status -cne 'Success' })
        if ($nonSuccessRows.Count -gt 0) {
            $statuses = @($nonSuccessRows.Status | Select-Object -Unique) -join ', '
            Add-VerificationError $Errors "capture report final status for project $number is not Success: $statuses"
        }

        $successOutputs = @($finalRows | Where-Object { [string]$_.Status -ceq 'Success' } | ForEach-Object { ([string]$_.Output).Replace('\', '/').TrimStart('./') })
        foreach ($mediaName in @([string]$project.image, [string]$project.gif)) {
            $expectedOutput = (($MediaDir.TrimEnd('/')) + '/' + $mediaName).Replace('\', '/')
            if ($expectedOutput -notin $successOutputs) {
                Add-VerificationError $Errors "capture report final Success outputs for project $number are incomplete: missing $expectedOutput"
            }
        }
    }
}

function Add-RootGifEmbed {
    param(
        [System.Collections.Generic.HashSet[string]]$Embeds,
        [string]$Target
    )

    $normalizedTarget = $Target.Trim()
    $suffixIndex = $normalizedTarget.IndexOfAny([char[]]'?#')
    if ($suffixIndex -ge 0) {
        $normalizedTarget = $normalizedTarget.Substring(0, $suffixIndex)
    }
    if ($normalizedTarget.EndsWith('.gif', [System.StringComparison]::OrdinalIgnoreCase)) {
        $null = $Embeds.Add($normalizedTarget)
    }
}

function Get-RootGifEmbeds {
    param([string]$Content)

    $embeds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $htmlPattern = '(?is)<img\b[^>]*?\bsrc\s*=\s*(?:"(?<double>[^"]+)"|''(?<single>[^'']+)''|(?<bare>[^\s>]+))'
    foreach ($match in [regex]::Matches($Content, $htmlPattern)) {
        $target = if ($match.Groups['double'].Success) { $match.Groups['double'].Value } elseif ($match.Groups['single'].Success) { $match.Groups['single'].Value } else { $match.Groups['bare'].Value }
        Add-RootGifEmbed -Embeds $embeds -Target $target
    }

    $markdownPattern = '(?is)!\[[^\]]*\]\(\s*(?:<(?<angle>[^>]+)>|(?<plain>[^\s\)]+))'
    foreach ($match in [regex]::Matches($Content, $markdownPattern)) {
        $target = if ($match.Groups['angle'].Success) { $match.Groups['angle'].Value } else { $match.Groups['plain'].Value }
        Add-RootGifEmbed -Embeds $embeds -Target $target
    }

    $referenceDefinitions = [System.Collections.Generic.Dictionary[string, string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $definitionPattern = '(?im)^\s{0,3}\[(?<id>[^\]]+)\]:\s*(?:<(?<angle>[^>]+)>|(?<plain>\S+))'
    foreach ($match in [regex]::Matches($Content, $definitionPattern)) {
        $target = if ($match.Groups['angle'].Success) { $match.Groups['angle'].Value } else { $match.Groups['plain'].Value }
        $referenceDefinitions[$match.Groups['id'].Value.Trim()] = $target
    }
    $referenceImagePattern = '(?is)!\[[^\]]*\]\[(?<id>[^\]]+)\]'
    foreach ($match in [regex]::Matches($Content, $referenceImagePattern)) {
        $referenceId = $match.Groups['id'].Value.Trim()
        if ($referenceDefinitions.ContainsKey($referenceId)) {
            Add-RootGifEmbed -Embeds $embeds -Target $referenceDefinitions[$referenceId]
        }
    }
    $collapsedReferenceImagePattern = '(?is)!\[(?<id>[^\]]+)\]\[\]'
    foreach ($match in [regex]::Matches($Content, $collapsedReferenceImagePattern)) {
        $referenceId = $match.Groups['id'].Value.Trim()
        if ($referenceDefinitions.ContainsKey($referenceId)) {
            Add-RootGifEmbed -Embeds $embeds -Target $referenceDefinitions[$referenceId]
        }
    }
    $shortcutReferenceImagePattern = '(?is)!\[(?<id>[^\]]+)\](?![\[\(])'
    foreach ($match in [regex]::Matches($Content, $shortcutReferenceImagePattern)) {
        $referenceId = $match.Groups['id'].Value.Trim()
        if ($referenceDefinitions.ContainsKey($referenceId)) {
            Add-RootGifEmbed -Embeds $embeds -Target $referenceDefinitions[$referenceId]
        }
    }

    return @($embeds)
}

function Test-RootReadme {
    param(
        [string]$Path,
        [object[]]$Projects,
        [string]$MediaDir,
        [System.Collections.Generic.List[string]]$Errors
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Add-VerificationError $Errors "Root README missing: $Path"
        return
    }

    try {
        $content = Get-ReadmeText $Path
    }
    catch {
        Add-VerificationError $Errors "Root README decode failed: $($_.Exception.Message)"
        return
    }

    $allowedRootAttachments = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $null = $allowedRootAttachments.Add('https://github.com/user-attachments/assets/3aafc53e-d6ae-492d-8680-b240c19f1f92')
    $null = $allowedRootAttachments.Add('https://github.com/user-attachments/assets/64a50e8e-5580-4e76-97d1-b500f9c5a8a2')
    $attachmentPattern = 'https://github\.com/user-attachments/assets/[^"''\s<>()]+'
    foreach ($attachment in [regex]::Matches($content, $attachmentPattern)) {
        if (-not $allowedRootAttachments.Contains($attachment.Value)) {
            Add-VerificationError $Errors "Root README has an unexpected root user-attachment: $($attachment.Value)"
        }
    }
    if ([regex]::Replace($content, $attachmentPattern, '') -match 'github\.com/user-attachments') {
        Add-VerificationError $Errors 'Root README has an unsupported root user-attachment reference'
    }

    $normalizedMediaDir = $MediaDir.Replace('\', '/').TrimEnd('/')
    $expectedFeaturedGifs = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $rootGifEmbeds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($gifEmbed in @(Get-RootGifEmbeds -Content $content)) { $null = $rootGifEmbeds.Add($gifEmbed) }
    foreach ($project in $Projects) {
        $pngReference = "$normalizedMediaDir/$($project.image)"
        if (-not $content.Contains($pngReference, [System.StringComparison]::Ordinal)) {
            Add-VerificationError $Errors "Root README missing PNG reference: $pngReference"
        }

        $gifReference = "$normalizedMediaDir/$($project.gif)"
        $isFeatured = $null -ne $project.PSObject.Properties['rootFeaturedGif'] -and [bool]$project.rootFeaturedGif
        if ($isFeatured) {
            $null = $expectedFeaturedGifs.Add($gifReference)
            if (-not $rootGifEmbeds.Contains($gifReference)) {
                Add-VerificationError $Errors "Root README missing featured GIF embed: $gifReference"
            }
        }
    }

    foreach ($gifReference in $rootGifEmbeds) {
        if (-not $expectedFeaturedGifs.Contains($gifReference)) {
            Add-VerificationError $Errors "Root README has an unexpected root GIF reference: $gifReference"
        }
    }
}

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Split-Path -Parent $PSScriptRoot
}
$RepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)
$errors = [System.Collections.Generic.List[string]]::new()

if (-not (Test-Path -LiteralPath $RepoRoot -PathType Container)) {
    Add-VerificationError $errors "Repository root not found: $RepoRoot"
}

if ([string]::IsNullOrWhiteSpace($Manifest)) {
    $Manifest = 'tools/readme_media_manifest.json'
}

$manifestData = $null
try {
    $manifestData = Get-ReadmeMediaManifest -ManifestPath $Manifest -RepoRoot $RepoRoot
}
catch {
    Add-VerificationError $errors $_.Exception.Message
}

if ($null -ne $manifestData) {
    $requiredDimensions = [ordered]@{
        captureWidth = 1600
        captureHeight = 900
        gifWidth = 800
        gifHeight = 450
        infoWidth = 1600
        infoHeight = 640
        gifMaxBytes = 5242880
    }
    foreach ($property in $requiredDimensions.Keys) {
        $actualValue = [int64]0
        if ($null -eq $manifestData.PSObject.Properties[$property] -or
            -not [int64]::TryParse([string]$manifestData.$property, [ref]$actualValue) -or
            $actualValue -ne $requiredDimensions[$property]) {
            Add-VerificationError $errors "Manifest $property must be exactly $($requiredDimensions[$property])"
        }
    }

    $projects = @($manifestData.projects)
    $mediaDirPath = Resolve-ContainedRelativePath -BasePath $RepoRoot -RelativePath ([string]$manifestData.mediaDir) -ContainmentRoot $RepoRoot -Label 'Manifest mediaDir' -Errors $errors
    $runtimeDirPath = Resolve-ContainedRelativePath -BasePath $RepoRoot -RelativePath ([string]$manifestData.runtimeDir) -ContainmentRoot $RepoRoot -Label 'Manifest runtimeDir' -Errors $errors
    $dx11Root = Join-Path $RepoRoot 'Dx11'
    $resolvedProjects = [System.Collections.Generic.List[object]]::new()

    for ($index = 0; $index -lt $projects.Count; $index++) {
        $project = $projects[$index]
        $number = if ($null -ne $project.PSObject.Properties['number']) { [string]$project.number } else { '<unknown>' }
        $projectDirectory = Resolve-ContainedRelativePath -BasePath $dx11Root -RelativePath ([string]$project.directory) -ContainmentRoot $RepoRoot -Label "Project $number directory" -Errors $errors
        $projectReadmePath = $null
        if ($null -ne $projectDirectory) {
            $projectReadmePath = Resolve-ContainedRelativePath -BasePath $projectDirectory -RelativePath 'README.md' -ContainmentRoot $RepoRoot -Label "Project $number README path" -Errors $errors
        }
        $imagePath = $null
        $gifPath = $null
        $infoPath = $null
        if ($null -ne $mediaDirPath) {
            $imagePath = Resolve-ContainedRelativePath -BasePath $mediaDirPath -RelativePath ([string]$project.image) -ContainmentRoot $RepoRoot -Label "Project $number image path" -Errors $errors
            $gifPath = Resolve-ContainedRelativePath -BasePath $mediaDirPath -RelativePath ([string]$project.gif) -ContainmentRoot $RepoRoot -Label "Project $number GIF path" -Errors $errors
            $infoPath = Resolve-ContainedRelativePath -BasePath $mediaDirPath -RelativePath ([string]$project.infoImage) -ContainmentRoot $RepoRoot -Label "Project $number info path" -Errors $errors
        }

        $resolvedProjects.Add([pscustomobject]@{
            ProjectDirectory = $projectDirectory
            ProjectReadme = $projectReadmePath
            Image = $imagePath
            Gif = $gifPath
            Info = $infoPath
        })
    }

    $hasRejectedProjectTarget = @($resolvedProjects | Where-Object { $null -eq $_.ProjectReadme }).Count -gt 0
    if ($null -ne $runtimeDirPath -and $null -ne $mediaDirPath -and -not $hasRejectedProjectTarget) {
        try {
            foreach ($manifestError in @(Test-ReadmeMediaManifest -Manifest $manifestData -RepoRoot $RepoRoot)) {
                Add-VerificationError $errors "Manifest: $manifestError"
            }
        }
        catch {
            Add-VerificationError $errors "Manifest validation failed: $($_.Exception.Message)"
        }
    }

    for ($index = 0; $index -lt $projects.Count; $index++) {
        $project = $projects[$index]
        $number = if ($null -ne $project.PSObject.Properties['number']) { [string]$project.number } else { '<unknown>' }
        $resolved = $resolvedProjects[$index]

        if ($null -ne $resolved.Image) {
            try { Test-PngMedia -Path $resolved.Image -ExpectedWidth 1600 -ExpectedHeight 900 -Label "Project $number PNG" -Errors $errors }
            catch { Add-VerificationError $errors "Project $number PNG validation failed: $($resolved.Image) ($($_.Exception.Message))" }
        }
        if ($null -ne $resolved.Info) {
            try { Test-PngMedia -Path $resolved.Info -ExpectedWidth 1600 -ExpectedHeight 640 -Label "Project $number info PNG" -Errors $errors }
            catch { Add-VerificationError $errors "Project $number info PNG validation failed: $($resolved.Info) ($($_.Exception.Message))" }
        }
        if ($null -ne $resolved.Gif) {
            try {
                $expectedGifFps = [int](Get-ReadmeMediaEffectivePositiveNumber -Manifest $manifestData -Project $project -Name 'gifFps')
                $expectedGifSeconds = Get-ReadmeMediaEffectivePositiveNumber -Manifest $manifestData -Project $project -Name 'gifSeconds'
                $expectedGifMaxBytes = [int64](Get-ReadmeMediaEffectivePositiveNumber -Manifest $manifestData -Project $project -Name 'gifMaxBytes')
                Test-GifMedia -Path $resolved.Gif -Label "Project $number GIF" -ExpectedFps $expectedGifFps -ExpectedSeconds $expectedGifSeconds -ExpectedMaxBytes $expectedGifMaxBytes -Errors $errors
            }
            catch { Add-VerificationError $errors "Project $number GIF validation failed: $($resolved.Gif) ($($_.Exception.Message))" }
        }
        if ($null -ne $resolved.ProjectReadme) {
            Test-ProjectReadme -Path $resolved.ProjectReadme -Project $project -Projects $projects -Index $index -Errors $errors
        }
    }

    $normalizedMediaDir = ([string]$manifestData.mediaDir).Replace('\', '/').TrimEnd('/')
    $rootReadmePath = Resolve-ContainedRelativePath -BasePath $RepoRoot -RelativePath 'README.md' -ContainmentRoot $RepoRoot -Label 'Root README path' -Errors $errors
    if ($null -ne $rootReadmePath) {
        Test-RootReadme -Path $rootReadmePath -Projects $projects -MediaDir $normalizedMediaDir -Errors $errors
    }
    if ($null -ne $mediaDirPath) {
        $reportPath = Resolve-ContainedRelativePath -BasePath $mediaDirPath -RelativePath 'capture-report.md' -ContainmentRoot $RepoRoot -Label 'Capture report path' -Errors $errors
        if ($null -ne $reportPath) {
            Test-CaptureReport -Path $reportPath -Projects $projects -MediaDir $normalizedMediaDir -Errors $errors
        }
    }
}

$oldReadmePath = Resolve-ContainedRelativePath -BasePath $RepoRoot -RelativePath 'README_old.md' -ContainmentRoot $RepoRoot -Label 'README_old path' -Errors $errors
if ($null -ne $oldReadmePath -and -not (Test-Path -LiteralPath $oldReadmePath -PathType Leaf)) {
    Add-VerificationError $errors 'README_old.md is missing'
}

if ($errors.Count -gt 0) {
    foreach ($verificationError in $errors) {
        Write-Output "ERROR: $verificationError"
    }
    exit 1
}

'README media verification passed'
