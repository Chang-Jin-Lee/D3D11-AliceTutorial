$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '..\readme_media_common.ps1')

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$captureScript = Join-Path $repoRoot 'tools\capture_readme_media.ps1'

$validation = @(. $captureScript -Manifest 'tools/readme_media_manifest.json' -ValidateOnly)
if ($validation -notcontains 'capture manifest validation passed') {
    throw 'capture manifest validation failed'
}

$mediaManifest = Get-ReadmeMediaManifest -ManifestPath 'tools/readme_media_manifest.json' -RepoRoot $repoRoot
$project36 = Get-CaptureProjectSelection -Manifest $mediaManifest -ProjectNumber '36'
Assert-True ($project36.Count -eq 1 -and $project36[0].number -eq '36') 'capture script did not select project 36'

try {
    $null = Get-CaptureProjectSelection -Manifest $mediaManifest -ProjectNumber '99'
    throw 'capture script accepted unknown project 99'
}
catch {
    if ($_.Exception.Message -notmatch 'Project number not found') { throw }
}

$project36ActionTypes = @($project36[0].preCaptureActions | ForEach-Object { $_.type })
Assert-True (($project36ActionTypes -join ',') -eq 'click,wait') 'project 36 start actions missing or reordered'
$sway = @($mediaManifest.projects | Where-Object { @($_.gifActions).Count -eq 4 })
Assert-True ($sway.Count -ge 1) 'camera sway actions missing'

$timelineActions = @(
    [pscustomobject]@{ atMs = 0; type = 'keyDown'; key = 'W' },
    [pscustomobject]@{ atMs = 124; type = 'keyDown'; key = 'A' },
    [pscustomobject]@{ atMs = 125; type = 'keyUp'; key = 'W' },
    [pscustomobject]@{ atMs = 126; type = 'keyUp'; key = 'A' },
    [pscustomobject]@{ atMs = 500; type = 'keyDown'; key = 'D' },
    [pscustomobject]@{ atMs = 501; type = 'keyUp'; key = 'D' }
)
$schedule = @(Get-GifActionSchedule -Actions $timelineActions -FrameCount 5 -FrameIntervalMs 125)
$scheduleSummary = @($schedule | ForEach-Object { "{0}:{1}:{2}" -f $_.FrameIndex, $_.FrameTimeMs, $_.Action.atMs })
$expectedSchedule = @('0:0:0', '1:125:124', '1:125:125', '2:250:126', '4:500:500')
Assert-True (($scheduleSummary -join ',') -eq ($expectedSchedule -join ',')) 'GIF action schedule did not use encoded frame boundaries exactly once'
Assert-True (@(Get-GifActionSchedule -Actions @() -FrameCount 5 -FrameIntervalMs 125).Count -eq 0) 'empty GIF actions must not create a dispatch entry'
Assert-True (@(Get-GifActionSchedule -Actions @($null) -FrameCount 5 -FrameIntervalMs 125).Count -eq 0) 'missing GIF actions must not create a dispatch entry'

$messageLog = [System.Collections.Generic.List[object]]::new()
$messageSink = {
    param([IntPtr]$Handle, [uint32]$Message, [IntPtr]$WParam, [IntPtr]$LParam)
    $messageLog.Add([pscustomobject]@{ Handle = $Handle; Message = $Message; WParam = $WParam; LParam = $LParam })
}.GetNewClosure()
$session = [pscustomobject]@{ Handle = [IntPtr]42; ClientWidth = 1600; ClientHeight = 900 }
$pressedKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

$noActionProjects = @(
    [pscustomobject]@{ Name = 'missing property'; Project = [pscustomobject]@{} },
    [pscustomobject]@{ Name = 'explicit null'; Project = [pscustomobject]@{ preCaptureActions = $null } },
    [pscustomobject]@{ Name = 'empty array'; Project = [pscustomobject]@{ preCaptureActions = @() } }
)
foreach ($case in $noActionProjects) {
    try {
        Invoke-PreCaptureActions -CaptureSession $session -Project $case.Project -PressedKeys $pressedKeys
    }
    catch {
        throw "pre-capture actions must ignore $($case.Name): $($_.Exception.Message)"
    }
}

$validActionKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$validActionProject = [pscustomobject]@{
    preCaptureActions = @([pscustomobject]@{ type = 'keyDown'; key = 'W' })
}
Invoke-PreCaptureActions -CaptureSession $session -Project $validActionProject -PressedKeys $validActionKeys
Assert-True ($validActionKeys.Contains('W')) 'valid pre-capture action was not dispatched'
Release-CaptureKeys -CaptureSession $session -PressedKeys $validActionKeys

Invoke-CaptureAction -CaptureSession $session -Action ([pscustomobject]@{ type = 'click'; x = 0.5; y = 0.5 }) -PressedKeys $pressedKeys -MessageSink $messageSink
Assert-True ($messageLog.Count -eq 2) 'one click must emit exactly two window messages'
Assert-True ($messageLog[0].Message -eq 0x0201 -and $messageLog[1].Message -eq 0x0202) 'one click must emit exactly one targeted down/up pair'

Invoke-CaptureAction -CaptureSession $session -Action ([pscustomobject]@{ type = 'keyDown'; key = 'D' }) -PressedKeys $pressedKeys -MessageSink $messageSink
Release-CaptureKeys -CaptureSession $session -PressedKeys $pressedKeys -MessageSink $messageSink
Assert-True ($pressedKeys.Count -eq 0) 'pressed-key cleanup did not drain tracked keys'
Assert-True (@($messageLog | Where-Object { $_.Message -eq [ReadmeCaptureWin32]::WM_KEYUP -and $_.WParam -eq [IntPtr]0x44 }).Count -eq 1) 'pressed-key cleanup did not emit key-up'

$tempRoot = Join-Path $env:TEMP ('D3D11-readme-capture-test-' + [Guid]::NewGuid().ToString('N'))
$tempRuntime = Join-Path $repoRoot ('Dx11\capture-test-runtime-' + [Guid]::NewGuid().ToString('N'))
$tempManifest = Join-Path $tempRoot 'manifest.json'
$tempOutput = Join-Path $tempRoot 'output'
New-Item -ItemType Directory -Path $tempRoot, $tempRuntime -Force | Out-Null
try {
    $missingExeManifest = $mediaManifest | ConvertTo-Json -Depth 32 | ConvertFrom-Json
    $missingExeManifest.runtimeDir = $tempRuntime
    $missingExeManifest | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath $tempManifest -Encoding UTF8

    $captureOutput = & pwsh -NoProfile -File $captureScript -Manifest $tempManifest -ProjectNumber 36 -SkipGif -OutputDir $tempOutput 2>&1
    Assert-True ($LASTEXITCODE -ne 0) 'missing executable capture returned success'
    $reportPath = Join-Path $tempOutput 'capture-report.md'
    Assert-True (Test-Path -LiteralPath $reportPath -PathType Leaf) 'missing executable capture did not write a report'
    $failedAttempts = @((Get-Content -Raw -LiteralPath $reportPath) -split "`r?`n" | Where-Object { $_ -match '^\| 36 \| [12] \| .* \| Failure \|' })
    Assert-True ($failedAttempts.Count -eq 2) 'captureAttempts=2 did not record two failed attempts'
}
finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $tempRuntime -Recurse -Force -ErrorAction SilentlyContinue
}

'capture action tests passed'
