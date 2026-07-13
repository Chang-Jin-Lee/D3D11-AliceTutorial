[CmdletBinding()]
param(
    [string]$RepoRoot,
    [string]$Manifest,
    [switch]$All
)

$ErrorActionPreference = 'Stop'

function Resolve-ProjectPath([string]$Path, [string]$Root) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $Root $Path))
}

function Get-ReadmeDocument([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $encoding = $null
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
        $content = $encoding.GetString($bytes, $offset, $bytes.Length - $offset)
    }
    catch [System.Text.DecoderFallbackException] {
        [System.Text.Encoding]::RegisterProvider([System.Text.CodePagesEncodingProvider]::Instance)
        $encoding = [System.Text.Encoding]::GetEncoding(949, [System.Text.EncoderExceptionFallback]::new(), [System.Text.DecoderExceptionFallback]::new())
        try {
            $content = $encoding.GetString($bytes)
        }
        catch [System.Text.DecoderFallbackException] {
            throw "README is not UTF-8, UTF-16, or CP949: $Path"
        }

        if (-not [System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($bytes, $encoding.GetBytes($content))) {
            throw "README CP949 bytes do not round-trip: $Path"
        }
    }

    return [pscustomobject]@{
        Content = $content
        Encoding = $encoding
    }
}

function Write-ReadmeDocument([string]$Path, [string]$Content, [System.Text.Encoding]$Encoding) {
    $encodedContent = $Encoding.GetBytes($Content)
    $preamble = $Encoding.GetPreamble()
    $bytes = [byte[]]::new($preamble.Length + $encodedContent.Length)
    [System.Array]::Copy($preamble, 0, $bytes, 0, $preamble.Length)
    [System.Array]::Copy($encodedContent, 0, $bytes, $preamble.Length, $encodedContent.Length)
    [System.IO.File]::WriteAllBytes($Path, $bytes)
}

function Get-Newline([string]$Content) {
    if ($Content.Contains("`r`n")) {
        return "`r`n"
    }

    if ($Content.Contains("`n")) {
        return "`n"
    }

    return [Environment]::NewLine
}

function Get-MarkerState([string]$Content, [string]$Name) {
    $start = "<!-- $Name`:START -->"
    $end = "<!-- $Name`:END -->"
    $startCount = 0
    $endCount = 0
    $searchIndex = 0

    while (($found = $Content.IndexOf($start, $searchIndex, [System.StringComparison]::Ordinal)) -ge 0) {
        $startCount++
        $searchIndex = $found + $start.Length
    }

    $searchIndex = 0
    while (($found = $Content.IndexOf($end, $searchIndex, [System.StringComparison]::Ordinal)) -ge 0) {
        $endCount++
        $searchIndex = $found + $end.Length
    }

    return [pscustomobject]@{
        Name = $Name
        Start = $start
        End = $end
        StartIndex = $Content.IndexOf($start, [System.StringComparison]::Ordinal)
        EndIndex = $Content.IndexOf($end, [System.StringComparison]::Ordinal)
        StartCount = $startCount
        EndCount = $endCount
    }
}

function Assert-GeneratedMarkerTopology([object[]]$States, [string]$Path) {
    $orderedNames = @('README-NAV-TOP', 'README-INFO', 'README-RUNTIME', 'README-NAV-BOTTOM')
    $stateByName = @{}
    foreach ($state in $States) {
        $stateByName[$state.Name] = $state
        if ($state.StartIndex -ge $state.EndIndex) {
            throw "README has reversed generated marker pair: $Path ($($state.Name))"
        }
    }

    for ($left = 0; $left -lt $States.Count; $left++) {
        for ($right = $left + 1; $right -lt $States.Count; $right++) {
            $first = $States[$left]
            $second = $States[$right]
            if ($first.StartIndex -lt $second.EndIndex -and $second.StartIndex -lt $first.EndIndex) {
                throw "README has overlapping generated marker intervals: $Path ($($first.Name), $($second.Name))"
            }
        }
    }

    $previousIndex = -1
    foreach ($name in $orderedNames) {
        $state = $stateByName[$name]
        foreach ($index in @($state.StartIndex, $state.EndIndex)) {
            if ($index -le $previousIndex) {
                throw "README generated markers are not in required order: $Path"
            }
            $previousIndex = $index
        }
    }
}

function Test-SafePathSegment([string]$Value) {
    if ([string]::IsNullOrEmpty($Value) -or [System.Text.RegularExpressions.Regex]::IsMatch($Value, '[\p{C}\s]')) {
        return $false
    }

    return -not [System.Text.RegularExpressions.Regex]::IsMatch($Value, '\A\.+\z') -and [System.Text.RegularExpressions.Regex]::IsMatch($Value, '\A[A-Za-z0-9._-]+\z')
}

function Assert-SafeProjectDirectory([string]$Directory, [object]$Project) {
    if (-not (Test-SafePathSegment $Directory)) {
        throw "Manifest project directory is not a safe path segment: $Directory ($($Project.number))"
    }
}

function Assert-SafeMediaPath([string]$Path, [string]$Property, [object]$Project) {
    if ([System.IO.Path]::IsPathRooted($Path) -or $Path.StartsWith('/') -or $Path.Contains('\')) {
        throw "Manifest project $Property is not a safe relative path: $Path ($($Project.number))"
    }

    $segments = [System.Text.RegularExpressions.Regex]::Split($Path, '/')
    if ($segments.Count -eq 0 -or @($segments | Where-Object { -not (Test-SafePathSegment $_) }).Count -ne 0) {
        throw "Manifest project $Property is not a safe relative path: $Path ($($Project.number))"
    }
}

function Replace-GeneratedBlock([string]$Content, [pscustomobject]$State, [string]$Block) {
    $startIndex = $Content.IndexOf($State.Start, [System.StringComparison]::Ordinal)
    $endIndex = $Content.IndexOf($State.End, $startIndex + $State.Start.Length, [System.StringComparison]::Ordinal)
    if ($endIndex -lt $startIndex) {
        throw "Malformed generated block: $($State.Name)"
    }

    $afterIndex = $endIndex + $State.End.Length
    return $Content.Substring(0, $startIndex) + $Block + $Content.Substring($afterIndex)
}

function New-NavigationBlock([string]$Marker, [object[]]$Projects, [int]$Index, [string]$Newline) {
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
    ) -join $Newline
}

function New-InfoBlock([object]$Project, [string]$Newline) {
    return @(
        '<!-- README-INFO:START -->'
        ('<p align="center"><img src="../../docs/media/readme/{0}" width="100%" /></p>' -f $Project.infoImage)
        '<!-- README-INFO:END -->'
    ) -join $Newline
}

function New-RuntimeBlock([object]$Project, [string]$Newline) {
    return @(
        '<!-- README-RUNTIME:START -->'
        '## 실행 화면'
        ''
        '| Screenshot | GIF |'
        '|---|---|'
        ('| <img src="../../docs/media/readme/{0}" width="100%" /> | <img src="../../docs/media/readme/{1}" width="100%" /> |' -f $Project.image, $Project.gif)
        '<!-- README-RUNTIME:END -->'
    ) -join $Newline
}

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Split-Path -Parent $PSScriptRoot
}
$RepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)

if ([string]::IsNullOrWhiteSpace($Manifest)) {
    $Manifest = 'tools/readme_media_manifest.json'
}
$manifestPath = Resolve-ProjectPath $Manifest $RepoRoot
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Manifest not found: $manifestPath"
}

$manifestData = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$projects = @($manifestData.projects)
if ($projects.Count -eq 0) {
    throw 'Manifest contains no projects.'
}
if ($null -ne $manifestData.expectedProjectCount -and [int]$manifestData.expectedProjectCount -ne $projects.Count) {
    throw "Manifest project count mismatch: expected $($manifestData.expectedProjectCount), found $($projects.Count)."
}

$requiredProperties = @('directory', 'image', 'gif', 'infoImage')
foreach ($project in $projects) {
    foreach ($property in $requiredProperties) {
        if ([string]::IsNullOrWhiteSpace([string]$project.$property)) {
            throw "Manifest project is missing $($property): $($project.number)"
        }
    }

    Assert-SafeProjectDirectory ([string]$project.directory) $project
    foreach ($property in @('image', 'gif', 'infoImage')) {
        Assert-SafeMediaPath ([string]$project.$property) $property $project
    }
}

$markerNames = @('README-NAV-TOP', 'README-INFO', 'README-RUNTIME', 'README-NAV-BOTTOM')
$updates = foreach ($index in 0..($projects.Count - 1)) {
    $project = $projects[$index]
    $readmePath = Join-Path $RepoRoot (Join-Path 'Dx11' (Join-Path $project.directory 'README.md'))
    if (-not (Test-Path -LiteralPath $readmePath -PathType Leaf)) {
        throw "Project README not found: $readmePath"
    }

    $document = Get-ReadmeDocument $readmePath
    $newline = Get-Newline $document.Content
    $blocks = @{
        'README-NAV-TOP' = New-NavigationBlock 'README-NAV-TOP' $projects $index $newline
        'README-INFO' = New-InfoBlock $project $newline
        'README-RUNTIME' = New-RuntimeBlock $project $newline
        'README-NAV-BOTTOM' = New-NavigationBlock 'README-NAV-BOTTOM' $projects $index $newline
    }
    $states = @($markerNames | ForEach-Object { Get-MarkerState $document.Content $_ })
    $hasNoBlocks = @($states | Where-Object { $_.StartCount -eq 0 -and $_.EndCount -eq 0 }).Count -eq $states.Count
    $hasCompleteBlocks = @($states | Where-Object { $_.StartCount -eq 1 -and $_.EndCount -eq 1 }).Count -eq $states.Count

    if (-not $hasNoBlocks -and -not $hasCompleteBlocks) {
        $invalidMarkers = $states | Where-Object { $_.StartCount -ne 1 -or $_.EndCount -ne 1 } | ForEach-Object { $_.Name }
        throw "README has incomplete or duplicate generated markers: $readmePath ($($invalidMarkers -join ', '))"
    }

    if ($hasCompleteBlocks) {
        Assert-GeneratedMarkerTopology $states $readmePath
    }

    if ($hasNoBlocks) {
        $body = $document.Content.TrimEnd([char[]]"`r`n")
        $newContent = @(
            $blocks['README-NAV-TOP']
            $blocks['README-INFO']
            $body
            $blocks['README-RUNTIME']
            $blocks['README-NAV-BOTTOM']
        ) -join ($newline + $newline)
        $newContent = $newContent.TrimEnd([char[]]"`r`n") + $newline
    }
    else {
        $newContent = $document.Content
        foreach ($state in $states) {
            $newContent = Replace-GeneratedBlock $newContent $state $blocks[$state.Name]
        }
        $newContent = $newContent.TrimEnd([char[]]"`r`n") + $newline
    }

    [pscustomobject]@{
        Path = $readmePath
        Document = $document
        Content = $newContent
    }
}

$changed = 0
foreach ($update in $updates) {
    if ($update.Content -cne $update.Document.Content) {
        Write-ReadmeDocument $update.Path $update.Content $update.Document.Encoding
        $changed++
    }
}

Write-Output "Updated $changed of $($updates.Count) project README files."
