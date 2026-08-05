$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Write-CanonicalReadme([string]$Path, [string]$ImagePath, [int]$Width) {
    $content = @(
        '# Fixture',
        '',
        '<!-- README-BRAND:START -->',
        "<p align=`"center`"><img src=`"$ImagePath`" width=`"$Width`" alt=`"D3D11 Alice Tutorial mascot logo`" /></p>",
        '<!-- README-BRAND:END -->',
        '',
        'Body'
    ) -join "`n"
    [IO.File]::WriteAllText($Path, $content + "`n", [Text.UTF8Encoding]::new($false))
}

function New-BaseFixture([string]$Path, [string]$AcceptanceScript, [string]$RepoRoot, [string[]]$Directories) {
    New-Item -ItemType Directory -Force -Path (Join-Path $Path 'tools\tests'), (Join-Path $Path 'Dx11\Resource\Icon'), (Join-Path $Path 'docs\media\branding') | Out-Null
    Copy-Item -LiteralPath $AcceptanceScript -Destination (Join-Path $Path 'tools\tests\test_app_branding.ps1')
    Copy-Item -LiteralPath (Join-Path $RepoRoot 'Dx11\Resource\Icon\AliceTutorialIcon.png') -Destination (Join-Path $Path 'Dx11\Resource\Icon\AliceTutorialIcon.png')
    Copy-Item -LiteralPath (Join-Path $RepoRoot 'Dx11\Resource\Icon\AliceTutorial.ico') -Destination (Join-Path $Path 'Dx11\Resource\Icon\AliceTutorial.ico')
    Copy-Item -LiteralPath (Join-Path $RepoRoot 'docs\media\branding\alice-tutorial-logo.png') -Destination (Join-Path $Path 'docs\media\branding\alice-tutorial-logo.png')

    $solutionLines = @('Project("{TYPE}") = "Common", "Common\Common.vcxproj", "{COMMON}"')
    $solutionLines += @($Directories | ForEach-Object { "Project(`"{TYPE}`") = `"$_`", `"$_\$_.vcxproj`", `"{PROJECT}`"" })
    [IO.File]::WriteAllText((Join-Path $Path 'Dx11\TutorialApp.sln'), ($solutionLines -join "`r`n") + "`r`n", [Text.UTF8Encoding]::new($false))
    @{ expectedProjectCount = 37; projects = @($Directories | ForEach-Object { @{ directory = $_ } }) } |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Path 'tools\readme_media_manifest.json') -Encoding utf8NoBOM
    Write-CanonicalReadme (Join-Path $Path 'README.md') 'docs/media/branding/alice-tutorial-logo.png' 720
    foreach ($directory in $Directories | Sort-Object -Unique) {
        New-Item -ItemType Directory -Force -Path (Join-Path $Path "Dx11\$directory") | Out-Null
        Write-CanonicalReadme (Join-Path $Path "Dx11\$directory\README.md") '../../docs/media/branding/alice-tutorial-logo.png' 520
    }
}

function Set-PngPixel([string]$Path, [int]$X, [int]$Y, [Drawing.Color]$Color) {
    Add-Type -AssemblyName System.Drawing
    $source = [Drawing.Bitmap]::new($Path)
    try {
        $copy = [Drawing.Bitmap]::new($source.Width, $source.Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [Drawing.Graphics]::FromImage($copy)
            try { $graphics.DrawImageUnscaled($source, 0, 0) }
            finally { $graphics.Dispose() }
            $copy.SetPixel($X, $Y, $Color)
            $temporary = $Path + '.rewrite.png'
            $copy.Save($temporary, [Drawing.Imaging.ImageFormat]::Png)
        }
        finally { $copy.Dispose() }
    }
    finally { $source.Dispose() }
    Move-Item -LiteralPath $temporary -Destination $Path -Force
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$acceptanceScript = Join-Path $repoRoot 'tools\tests\test_app_branding.ps1'
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('app-brand-acceptance-' + [guid]::NewGuid().ToString('N'))
$canonicalDirectories = @(1..37 | ForEach-Object { '{0:D2}_Test' -f $_ })
$failures = [Collections.Generic.List[string]]::new()

try {
    $cases = @(
        [pscustomobject]@{
            Name = 'duplicate manifest directory'
            Pattern = 'manifest project directories must be unique'
            Mutate = {
                param($path)
                $directories = @($canonicalDirectories[0..35]) + @($canonicalDirectories[0])
                @{ expectedProjectCount = 37; projects = @($directories | ForEach-Object { @{ directory = $_ } }) } |
                    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $path 'tools\readme_media_manifest.json') -Encoding utf8NoBOM
            }
        },
        [pscustomobject]@{
            Name = 'solution-external manifest substitution'
            Pattern = 'manifest project directories must exactly match TutorialApp.sln application projects'
            Mutate = {
                param($path)
                $directories = @($canonicalDirectories[0..35]) + @('99_External')
                @{ expectedProjectCount = 37; projects = @($directories | ForEach-Object { @{ directory = $_ } }) } |
                    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $path 'tools\readme_media_manifest.json') -Encoding utf8NoBOM
                New-Item -ItemType Directory -Force -Path (Join-Path $path 'Dx11\99_External') | Out-Null
                Write-CanonicalReadme (Join-Path $path 'Dx11\99_External\README.md') '../../docs/media/branding/alice-tutorial-logo.png' 520
            }
        },
        [pscustomobject]@{
            Name = 'immediate root blockquote follower'
            Pattern = 'brand block must have exactly one trailing blank line'
            Mutate = {
                param($path)
                $readme = Join-Path $path 'README.md'
                $content = [IO.File]::ReadAllText($readme).Replace("<!-- README-BRAND:END -->`n`nBody", "<!-- README-BRAND:END -->`n> Quote")
                [IO.File]::WriteAllText($readme, $content, [Text.UTF8Encoding]::new($false))
            }
        },
        [pscustomobject]@{
            Name = 'immediate project list follower'
            Pattern = 'brand block must have exactly one trailing blank line'
            Mutate = {
                param($path)
                $readme = Join-Path $path 'Dx11\01_Test\README.md'
                $content = [IO.File]::ReadAllText($readme).Replace("<!-- README-BRAND:END -->`n`nBody", "<!-- README-BRAND:END -->`n- Item")
                [IO.File]::WriteAllText($readme, $content, [Text.UTF8Encoding]::new($false))
            }
        },
        [pscustomobject]@{
            Name = 'master PNG dimensions and mode'
            Pattern = 'master PNG must be 1024x1024 RGBA'
            Mutate = { param($path) Copy-Item -LiteralPath (Join-Path $path 'docs\media\branding\alice-tutorial-logo.png') -Destination (Join-Path $path 'Dx11\Resource\Icon\AliceTutorialIcon.png') -Force }
        },
        [pscustomobject]@{
            Name = 'banner PNG dimensions and mode'
            Pattern = 'README banner must be 1536x640 RGB'
            Mutate = { param($path) Copy-Item -LiteralPath (Join-Path $path 'Dx11\Resource\Icon\AliceTutorialIcon.png') -Destination (Join-Path $path 'docs\media\branding\alice-tutorial-logo.png') -Force }
        },
        [pscustomobject]@{
            Name = 'master transparent top corner'
            Pattern = 'master PNG top corners must be transparent'
            Mutate = { param($path) Set-PngPixel (Join-Path $path 'Dx11\Resource\Icon\AliceTutorialIcon.png') 0 0 ([Drawing.Color]::FromArgb(255, 255, 255, 255)) }
        },
        [pscustomobject]@{
            Name = 'master foreground alpha hole'
            Pattern = 'master PNG foreground clothing must be fully opaque'
            Mutate = { param($path) Set-PngPixel (Join-Path $path 'Dx11\Resource\Icon\AliceTutorialIcon.png') 500 1000 ([Drawing.Color]::FromArgb(0, 255, 255, 255)) }
        },
        [pscustomobject]@{
            Name = 'ICO directory entries'
            Pattern = 'ICO must contain exactly the nine required square entries'
            Mutate = { param($path) [IO.File]::WriteAllBytes((Join-Path $path 'Dx11\Resource\Icon\AliceTutorial.ico'), [byte[]](0..31)) }
        }
    )

    foreach ($case in $cases) {
        $casePath = Join-Path $fixtureRoot ($case.Name -replace '[^A-Za-z0-9]+', '-')
        New-BaseFixture $casePath $acceptanceScript $repoRoot $canonicalDirectories
        & $case.Mutate $casePath
        $childOutput = & (Get-Command pwsh).Source -NoProfile -File (Join-Path $casePath 'tools\tests\test_app_branding.ps1') 2>&1 | Out-String
        $childExit = $LASTEXITCODE
        "acceptance-negative name=$($case.Name) exit=$childExit"
        if ($childExit -eq 0 -or $childOutput -notmatch [regex]::Escape($case.Pattern)) {
            $failures.Add("$($case.Name) was not rejected with '$($case.Pattern)' (exit=$childExit; output=$($childOutput.Trim()))")
        }
    }

    Assert-True ($failures.Count -eq 0) ($failures -join "`n")
    'app branding acceptance negative tests passed'
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) { Remove-Item -LiteralPath $fixtureRoot -Recurse -Force }
}
