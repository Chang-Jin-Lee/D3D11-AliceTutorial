$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '..\readme_media_common.ps1')

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

# A real, decodable PNG of a requested size. The backbuffer provider round-trip
# below has to work on actual image bytes, not on a placeholder file.
function New-TestPng {
    param([string]$Path, [int]$Width, [int]$Height)

    $bitmap = $null
    $graphics = $null
    try {
        $bitmap = New-Object System.Drawing.Bitmap($Width, $Height)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.Clear([System.Drawing.Color]::FromArgb(255, 18, 24, 40))
        $graphics.FillRectangle([System.Drawing.Brushes]::Goldenrod, 12, 12,
            [Math]::Max(1, [int]($Width / 3)), [Math]::Max(1, [int]($Height / 3)))
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($null -ne $graphics) { $graphics.Dispose() }
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

function Invoke-TestPwshProcess {
    param(
        [Parameter(Mandatory)][string]$ScriptPath,
        [string[]]$Arguments = @(),
        [switch]$InheritOutput
    )

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = (Get-Process -Id $PID).Path
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = -not $InheritOutput
    $startInfo.RedirectStandardError = -not $InheritOutput
    foreach ($argument in @('-NoLogo', '-NoProfile', '-NonInteractive', '-File', $ScriptPath) + $Arguments) {
        [void]$startInfo.ArgumentList.Add([string]$argument)
    }

    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        [void]$process.Start()
        if (-not $InheritOutput) {
            $standardOutput = $process.StandardOutput.ReadToEndAsync()
            $standardError = $process.StandardError.ReadToEndAsync()
        }
        $process.WaitForExit()
        return [pscustomobject]@{
            ExitCode = $process.ExitCode
            Output = if ($InheritOutput) {
                ''
            }
            else {
                ($standardOutput.GetAwaiter().GetResult() + $standardError.GetAwaiter().GetResult()).Trim()
            }
        }
    }
    finally {
        $process.Dispose()
    }
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
$resetActions = @($project36[0].gifActions)
Assert-True ($resetActions.Count -eq 1) 'project 36 needs exactly one GIF reset action'
Assert-True ($resetActions[0].type -eq 'click' -and [int]$resetActions[0].atMs -eq 0) 'project 36 reset must be a click at frame zero'
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
    # Ticks is a high-resolution stamp of when the tool asked for this message to
    # go out, which is what the click-hold assertion below measures.
    $messageLog.Add([pscustomobject]@{
        Handle = $Handle; Message = $Message; WParam = $WParam; LParam = $LParam
        Ticks = [System.Diagnostics.Stopwatch]::GetTimestamp()
    })
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

# Emitting the pair is not enough: the target has to be able to OBSERVE a press
# edge. GameApp::Run (Dx11/Common/GameApp.cpp) drains the whole message queue
# before it calls Update() once, and DirectXTK's Mouse keeps a level state, so a
# down and an up delivered in the same drain leave leftButton false when
# InputSystem::Update samples it and Mouse::ButtonStateTracker never reports
# PRESSED. Project 36's frame-zero showcase reset is exactly such a click, and
# when it is missed every phase index in
# tools/tests/test_project36_portfolio_media.ps1 addresses the wrong runtime
# window while still passing - so the miss is invisible unless something here
# catches it.
#
# WHAT THIS PROVES: the tool leaves at least one 60 Hz frame between the down and
# the up, so an edit that collapses them back to back - which is what the tool
# used to do - fails here. WHAT IT DOES NOT PROVE: that any application actually
# observed the edge. There is no window, no message pump and no InputSystem in
# this test; the sink is a script block and the handle is a made-up IntPtr. The
# end-to-end evidence stays where it has to be, in the GUI test
# tools/tests/test_project36_portfolio_showcase.ps1, which drives the real window
# and reads the showcase clock back out of the HUD.
$minimumClickHoldMs = 16.0
$clickHoldMs = ($messageLog[1].Ticks - $messageLog[0].Ticks) * 1000.0 / [System.Diagnostics.Stopwatch]::Frequency
Assert-True ($clickHoldMs -ge $minimumClickHoldMs) `
    ("a click must hold the button down across an application frame: measured {0:N1} ms between down and up (need >= {1:N0} ms)" -f $clickHoldMs, $minimumClickHoldMs)

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
    # All retained applications share one runtime imgui.ini, so a retained batch
    # cannot safely compose independent deferred restorers. The CLI must reject
    # the combination before touching the user's bytes, creating a backup/output,
    # or retaining any capture process. -ValidateOnly keeps this RED test safe
    # even against the pre-fix entrypoint.
    $unsafeBatchManifest = $mediaManifest | ConvertTo-Json -Depth 32 | ConvertFrom-Json
    $unsafeBatchManifest.runtimeDir = [IO.Path]::GetRelativePath($repoRoot, $tempRuntime)
    $unsafeBatchManifest | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath $tempManifest -Encoding UTF8
    $unsafeBatchIniPath = Join-Path $tempRuntime 'imgui.ini'
    $unsafeBatchOriginalBytes = [byte[]](0x00, 0x4B, 0x65, 0x65, 0x70, 0xFF)
    [IO.File]::WriteAllBytes($unsafeBatchIniPath, $unsafeBatchOriginalBytes)
    $unsafeBatchOutputDir = Join-Path $tempRoot 'unsafe batch output'
    $unsafeBatchOutput = & pwsh -NoProfile -File $captureScript -Manifest $tempManifest `
        -All -KeepWindows -ValidateOnly -OutputDir $unsafeBatchOutputDir 2>&1
    $unsafeBatchExitCode = $LASTEXITCODE
    $unsafeBatchMessage = (($unsafeBatchOutput | Out-String) -replace '\x1b\[[0-9;]*m', '') `
        -replace '(?m)^[^\S\r\n]*\|[^\S\r\n]*', '' -replace '\s+', ' '
    Assert-True ($unsafeBatchExitCode -ne 0) 'capture CLI accepted unsafe -All -KeepWindows combination'
    Assert-True ($unsafeBatchMessage -match 'Select a single project\s*\|?\s*with -ProjectNumber when using -KeepWindows') `
        'unsafe retained batch rejection did not explain how to select one project'
    Assert-True ([Linq.Enumerable]::SequenceEqual[byte](
            [IO.File]::ReadAllBytes($unsafeBatchIniPath), $unsafeBatchOriginalBytes)) `
        'unsafe retained batch rejection changed the user imgui.ini bytes'
    Assert-True (@(Get-ChildItem -LiteralPath $tempRuntime -Filter '.imgui.ini.readme-capture-*' -File).Count -eq 0) `
        'unsafe retained batch rejection created an imgui.ini backup or watcher file'
    Assert-True (-not (Test-Path -LiteralPath $unsafeBatchOutputDir)) `
        'unsafe retained batch rejection created capture output'
    Assert-True ($unsafeBatchMessage -notmatch 'Capturing project') `
        'unsafe retained batch rejection reached the capture process launch loop'

    # Capture isolation is observable through real filesystem and environment
    # side effects. A user's remembered ImGui layout must disappear while the
    # child starts, then return byte-for-byte even when launch/capture fails.
    $imguiIniPath = Join-Path $tempRuntime 'imgui.ini'
    $originalIni = "[Window][Controls]`nPos=913,777`nCollapsed=1`n"
    [IO.File]::WriteAllText($imguiIniPath, $originalIni)
    $iniState = Suspend-ReadmeCaptureImGuiIni -RuntimeDir $tempRuntime
    Assert-True (-not (Test-Path -LiteralPath $imguiIniPath)) 'capture did not isolate the existing imgui.ini'
    [IO.File]::WriteAllText($imguiIniPath, "capture-generated layout`n")
    Restore-ReadmeCaptureImGuiIni -State $iniState
    Assert-True (([IO.File]::ReadAllText($imguiIniPath)) -ceq $originalIni) `
        'capture did not restore the existing imgui.ini byte-for-byte'
    Assert-True (-not (Test-Path -LiteralPath $iniState.BackupPath)) `
        'capture left the recoverable imgui.ini backup behind after restoration'

    Remove-Item -LiteralPath $imguiIniPath -Force
    $emptyIniState = Suspend-ReadmeCaptureImGuiIni -RuntimeDir $tempRuntime
    [IO.File]::WriteAllText($imguiIniPath, "capture-generated layout`n")
    Restore-ReadmeCaptureImGuiIni -State $emptyIniState
    Assert-True (-not (Test-Path -LiteralPath $imguiIniPath)) `
        'capture without a pre-existing imgui.ini leaked generated layout state'

    # A synchronous restore can fail after the user's original layout has
    # already moved to its recoverable backup. Keep real bytes and a real
    # FileShare.None handle here: releasing ownership in this state would let
    # another invocation treat the generated layout as the user's original.
    $restoreFailureRoot = Join-Path $tempRoot 'synchronous restore failure'
    $restoreFailureRuntime = Join-Path $restoreFailureRoot 'runtime with spaces'
    New-Item -ItemType Directory -Path $restoreFailureRuntime -Force | Out-Null
    $restoreFailureIniPath = Join-Path $restoreFailureRuntime 'imgui.ini'
    $restoreFailureOriginalBytes = [byte[]](0x00, 0x52, 0x65, 0x73, 0x74, 0x6F, 0x72, 0x65, 0xFF)
    $restoreFailureGeneratedBytes = [byte[]](0x63, 0x61, 0x70, 0x74, 0x75, 0x72, 0x65)
    [IO.File]::WriteAllBytes($restoreFailureIniPath, $restoreFailureOriginalBytes)
    $restoreFailureLock = Enter-ReadmeCaptureRuntimeLock -RuntimeDir $restoreFailureRuntime
    if ($null -eq $restoreFailureLock.PSObject.Properties['RestorationPending']) {
        $restoreFailureLock | Add-Member -NotePropertyName RestorationPending -NotePropertyValue $true
    }
    else {
        $restoreFailureLock.RestorationPending = $true
    }
    $restoreFailureState = $null
    $exclusiveIniHandle = $null
    try {
        $restoreFailureState = Suspend-ReadmeCaptureImGuiIni -RuntimeDir $restoreFailureRuntime
        [IO.File]::WriteAllBytes($restoreFailureIniPath, $restoreFailureGeneratedBytes)
        $exclusiveIniHandle = [IO.File]::Open($restoreFailureIniPath, [IO.FileMode]::Open,
            [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)

        $restoreFailure = $null
        try { Restore-ReadmeCaptureImGuiIni -State $restoreFailureState }
        catch { $restoreFailure = $_ }
        Assert-True ($null -ne $restoreFailure) 'FileShare.None did not force synchronous imgui.ini restoration failure'
        Assert-True ($restoreFailure.Exception.Message -match 'imgui\.ini|used by another process|access') `
            "synchronous restore failure lost its actionable original diagnostic: $($restoreFailure.Exception.Message)"
        Assert-True (Test-Path -LiteralPath $restoreFailureState.BackupPath -PathType Leaf) `
            'failed synchronous restore lost the recoverable original backup'
        Assert-True ([Linq.Enumerable]::SequenceEqual[byte](
                [IO.File]::ReadAllBytes($restoreFailureState.BackupPath), $restoreFailureOriginalBytes)) `
            'failed synchronous restore changed the recoverable original bytes'

        $unlockFailure = $null
        try { Exit-ReadmeCaptureRuntimeLock -State $restoreFailureLock }
        catch { $unlockFailure = $_ }
        Assert-True (Test-Path -LiteralPath $restoreFailureLock.LockPath -PathType Leaf) `
            'synchronous restore failure released the runtime lock while original state remained unresolved'
        Assert-True ($null -ne $unlockFailure -and $unlockFailure.Exception.Message -match 'restoration is unresolved') `
            'unresolved restoration did not fail closed with an actionable lock diagnostic'
        Assert-True (-not [bool]$restoreFailureLock.Released) `
            'unresolved restoration marked its runtime lock released'
        Assert-True (([IO.File]::ReadAllText($restoreFailureLock.LockPath)) -ceq [string]$restoreFailureLock.Token) `
            'unresolved restoration changed the owned runtime-lock token'

        $exclusiveIniHandle.Dispose()
        $exclusiveIniHandle = $null
        Restore-ReadmeCaptureImGuiIni -State $restoreFailureState -RuntimeLockState $restoreFailureLock
        Assert-True (-not [bool]$restoreFailureLock.RestorationPending) `
            'successful explicit recovery did not resolve the restoration-pending state'
        Exit-ReadmeCaptureRuntimeLock -State $restoreFailureLock
        Assert-True ([Linq.Enumerable]::SequenceEqual[byte](
                [IO.File]::ReadAllBytes($restoreFailureIniPath), $restoreFailureOriginalBytes)) `
            'explicit recovery did not restore the original imgui.ini bytes'
        Assert-True (-not (Test-Path -LiteralPath $restoreFailureState.BackupPath)) `
            'explicit recovery left the original imgui.ini backup behind'
        Assert-True (-not (Test-Path -LiteralPath $restoreFailureLock.LockPath)) `
            'explicit recovery left the runtime lock behind'

        $successfulStateLock = Enter-ReadmeCaptureRuntimeLock -RuntimeDir $restoreFailureRuntime
        try {
            Assert-True ($null -ne $successfulStateLock.PSObject.Properties['RestorationPending'] -and
                -not [bool]$successfulStateLock.RestorationPending) `
                'new runtime ownership did not initialize explicit restoration state'
            $successfulIniState = Suspend-ReadmeCaptureImGuiIni -RuntimeDir $restoreFailureRuntime `
                -RuntimeLockState $successfulStateLock
            Assert-True ([bool]$successfulStateLock.RestorationPending) `
                'suspension did not mark imgui.ini restoration pending before state changed'
            [IO.File]::WriteAllBytes($restoreFailureIniPath, $restoreFailureGeneratedBytes)
            Restore-ReadmeCaptureImGuiIni -State $successfulIniState -RuntimeLockState $successfulStateLock
            Assert-True (-not [bool]$successfulStateLock.RestorationPending) `
                'successful synchronous restoration did not clear its pending state'
        }
        finally {
            Exit-ReadmeCaptureRuntimeLock -State $successfulStateLock
        }
    }
    finally {
        if ($null -ne $exclusiveIniHandle) { $exclusiveIniHandle.Dispose() }
        if (Test-Path -LiteralPath $restoreFailureLock.LockPath -PathType Leaf) {
            [IO.File]::Delete([string]$restoreFailureLock.LockPath)
        }
        if ($null -ne $restoreFailureState -and
            (Test-Path -LiteralPath $restoreFailureState.BackupPath -PathType Leaf)) {
            if (Test-Path -LiteralPath $restoreFailureIniPath) {
                [IO.File]::Delete($restoreFailureIniPath)
            }
            [IO.File]::Move($restoreFailureState.BackupPath, $restoreFailureIniPath)
        }
    }

    # Cross-invocation ownership has to survive the first pwsh owner exiting
    # while its retained child and detached restorer remain alive. The second
    # independent owner must fail before changing any bytes or artifacts.
    $ownerRoot = Join-Path $tempRoot 'independent retained owners'
    $ownerRuntime = Join-Path $ownerRoot 'runtime with spaces'
    $ownerMedia = Join-Path $ownerRoot 'media with spaces'
    $ownerChildScript = Join-Path $ownerRoot 'retained child.ps1'
    $retainedOwnerScript = Join-Path $ownerRoot 'retained owner.ps1'
    $probeOwnerScript = Join-Path $ownerRoot 'probe owner.ps1'
    $ownerReleasePath = Join-Path $ownerRoot 'release child'
    $ownerIdentityPath = Join-Path $ownerRoot 'retained identity.txt'
    $ownerShutdownPath = Join-Path $ownerRoot 'child shutdown.txt'
    $probeSuccessPath = Join-Path $ownerRoot 'probe acquired.txt'
    $ownerIniPath = Join-Path $ownerRuntime 'imgui.ini'
    $ownerBackbufferPath = Join-Path $ownerMedia 'backbuffer-99-0123456789abcdef0123456789abcdef.png'
    $ownerOriginalBytes = [byte[]](0x00, 0x31, 0x52, 0x73, 0x94, 0xB5, 0xD6, 0xF7)
    New-Item -ItemType Directory -Path $ownerRuntime, $ownerMedia -Force | Out-Null
    [IO.File]::WriteAllBytes($ownerIniPath, $ownerOriginalBytes)
    @'
param([string]$ReleasePath, [string]$IniPath, [string]$BackbufferPath, [string]$ShutdownPath)
$ErrorActionPreference = 'Stop'
while (-not (Test-Path -LiteralPath $ReleasePath -PathType Leaf)) { Start-Sleep -Milliseconds 25 }
[IO.File]::WriteAllBytes($IniPath, [byte[]](0x63, 0x61, 0x70, 0x74, 0x75, 0x72, 0x65))
[IO.File]::WriteAllBytes($BackbufferPath, [byte[]](0x62, 0x61, 0x63, 0x6B, 0x62, 0x75, 0x66, 0x66, 0x65, 0x72))
[IO.File]::WriteAllBytes(($BackbufferPath + '.tmp.png'), [byte[]](0x74, 0x65, 0x6D, 0x70))
[IO.File]::WriteAllText($ShutdownPath, 'written', [Text.UTF8Encoding]::new($false))
Start-Sleep -Milliseconds 250
'@ | Set-Content -LiteralPath $ownerChildScript -Encoding UTF8
    @'
param(
    [string]$CaptureScript, [string]$RepoRoot, [string]$RuntimeDir, [string]$MediaDir,
    [string]$ChildScript, [string]$ReleasePath, [string]$IdentityPath,
    [string]$ShutdownPath, [string]$BackbufferPath
)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $RepoRoot
. $CaptureScript -Manifest 'tools/readme_media_manifest.json' -ValidateOnly | Out-Null
$runtimeLock = $null
$iniState = $null
$child = $null
$handedOff = $false
try {
    $runtimeLock = Enter-ReadmeCaptureRuntimeLock -RuntimeDir $RuntimeDir
    $iniState = Suspend-ReadmeCaptureImGuiIni -RuntimeDir $RuntimeDir -RuntimeLockState $runtimeLock
    [IO.File]::WriteAllBytes($iniState.IniPath, [byte[]](0x63, 0x61, 0x70, 0x74, 0x75, 0x72, 0x65))
    [IO.File]::WriteAllBytes($BackbufferPath, [byte[]](0x69, 0x6E, 0x69, 0x74, 0x69, 0x61, 0x6C))
    [IO.File]::WriteAllBytes(($BackbufferPath + '.tmp.png'), [byte[]](0x74, 0x65, 0x6D, 0x70))

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = (Get-Process -Id $PID).Path
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in @(
        '-NoLogo', '-NoProfile', '-NonInteractive', '-File', $ChildScript,
        '-ReleasePath', $ReleasePath, '-IniPath', $iniState.IniPath,
        '-BackbufferPath', $BackbufferPath, '-ShutdownPath', $ShutdownPath
    )) { [void]$startInfo.ArgumentList.Add([string]$argument) }
    $child = [Diagnostics.Process]::Start($startInfo)
    $identity = '{0}|{1}' -f $child.Id, $child.StartTime.ToUniversalTime().Ticks
    Start-DeferredReadmeCaptureImGuiIniRestore -State $iniState -Process $child `
        -RuntimeLockState $runtimeLock -BackbufferPath $BackbufferPath -MediaDir $MediaDir
    $handedOff = $true
    [IO.File]::WriteAllText($IdentityPath, $identity, [Text.UTF8Encoding]::new($false))
}
finally {
    if (-not $handedOff) {
        if ($null -ne $child) {
            try {
                $child.Refresh()
                if (-not $child.HasExited) { $child.Kill(); [void]$child.WaitForExit(5000) }
            }
            catch {}
        }
        if ($null -ne $iniState) {
            Restore-ReadmeCaptureImGuiIni -State $iniState -RuntimeLockState $runtimeLock
        }
        if ($null -ne $runtimeLock) { Exit-ReadmeCaptureRuntimeLock -State $runtimeLock }
    }
    if ($null -ne $child) { $child.Dispose() }
}
'@ | Set-Content -LiteralPath $retainedOwnerScript -Encoding UTF8
    @'
param([string]$CaptureScript, [string]$RepoRoot, [string]$RuntimeDir, [string]$SuccessPath)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $RepoRoot
. $CaptureScript -Manifest 'tools/readme_media_manifest.json' -ValidateOnly | Out-Null
$runtimeLock = Enter-ReadmeCaptureRuntimeLock -RuntimeDir $RuntimeDir
try { [IO.File]::WriteAllText($SuccessPath, 'acquired', [Text.UTF8Encoding]::new($false)) }
finally { Exit-ReadmeCaptureRuntimeLock -State $runtimeLock }
'@ | Set-Content -LiteralPath $probeOwnerScript -Encoding UTF8

    $ownerChildId = $null
    $ownerChildStartTimeUtcTicks = $null
    try {
        $retainedOwnerResult = Invoke-TestPwshProcess -ScriptPath $retainedOwnerScript -InheritOutput -Arguments @(
            '-CaptureScript', $captureScript, '-RepoRoot', $repoRoot,
            '-RuntimeDir', $ownerRuntime, '-MediaDir', $ownerMedia,
            '-ChildScript', $ownerChildScript, '-ReleasePath', $ownerReleasePath,
            '-IdentityPath', $ownerIdentityPath, '-ShutdownPath', $ownerShutdownPath,
            '-BackbufferPath', $ownerBackbufferPath
        )
        Assert-True ($retainedOwnerResult.ExitCode -eq 0) `
            "first independent owner did not hand off its retained capture: $($retainedOwnerResult.Output)"
        $ownerIdentity = [IO.File]::ReadAllText($ownerIdentityPath).Split('|')
        $ownerChildId = [int]$ownerIdentity[0]
        $ownerChildStartTimeUtcTicks = [long]$ownerIdentity[1]
        $ownerChild = Get-Process -Id $ownerChildId -ErrorAction SilentlyContinue
        $ownerChildIsExact = $null -ne $ownerChild -and
            $ownerChild.StartTime.ToUniversalTime().Ticks -eq $ownerChildStartTimeUtcTicks
        Assert-True $ownerChildIsExact 'first independent owner did not leave its exact retained child alive'
        if ($null -ne $ownerChild) { $ownerChild.Dispose() }

        $beforeProbeIniBytes = [IO.File]::ReadAllBytes($ownerIniPath)
        $beforeProbeBackupPaths = @(Get-ChildItem -LiteralPath $ownerRuntime -Filter '.imgui.ini.readme-capture-*.bak' -File | Select-Object -ExpandProperty FullName)
        Assert-True ($beforeProbeBackupPaths.Count -eq 1) 'first owner did not leave exactly one recoverable imgui.ini backup'
        $beforeProbeBackupBytes = [IO.File]::ReadAllBytes($beforeProbeBackupPaths[0])
        $beforeProbeRuntimeNames = @(Get-ChildItem -LiteralPath $ownerRuntime -Force | Select-Object -ExpandProperty Name | Sort-Object)
        $beforeProbeMediaNames = @(Get-ChildItem -LiteralPath $ownerMedia -Force | Select-Object -ExpandProperty Name | Sort-Object)
        $beforeProbeBackbufferBytes = [IO.File]::ReadAllBytes($ownerBackbufferPath)
        $beforeProbeTempBytes = [IO.File]::ReadAllBytes($ownerBackbufferPath + '.tmp.png')

        $rejectedProbe = Invoke-TestPwshProcess -ScriptPath $probeOwnerScript -Arguments @(
            '-CaptureScript', $captureScript, '-RepoRoot', $repoRoot,
            '-RuntimeDir', $ownerRuntime, '-SuccessPath', $probeSuccessPath
        )
        Assert-True ($rejectedProbe.ExitCode -ne 0) 'second independent owner acquired an active retained-capture runtime lock'
        Assert-True ($rejectedProbe.Output -match 'already owns this runtime.*Close the retained capture window or wait') `
            "active runtime-lock rejection was not actionable: $($rejectedProbe.Output)"
        Assert-True (-not (Test-Path -LiteralPath $probeSuccessPath)) 'rejected second owner created its success artifact'
        Assert-True ([Linq.Enumerable]::SequenceEqual[byte]([IO.File]::ReadAllBytes($ownerIniPath), $beforeProbeIniBytes)) `
            'rejected second owner changed capture imgui.ini bytes'
        Assert-True ([Linq.Enumerable]::SequenceEqual[byte]([IO.File]::ReadAllBytes($beforeProbeBackupPaths[0]), $beforeProbeBackupBytes)) `
            'rejected second owner changed the recoverable imgui.ini backup bytes'
        Assert-True ((@(Get-ChildItem -LiteralPath $ownerRuntime -Force | Select-Object -ExpandProperty Name | Sort-Object) -join '|') -ceq ($beforeProbeRuntimeNames -join '|')) `
            'rejected second owner changed runtime lock/backup/ready artifacts'
        Assert-True ((@(Get-ChildItem -LiteralPath $ownerMedia -Force | Select-Object -ExpandProperty Name | Sort-Object) -join '|') -ceq ($beforeProbeMediaNames -join '|')) `
            'rejected second owner changed capture output artifacts'
        Assert-True ([Linq.Enumerable]::SequenceEqual[byte]([IO.File]::ReadAllBytes($ownerBackbufferPath), $beforeProbeBackbufferBytes) -and
            [Linq.Enumerable]::SequenceEqual[byte]([IO.File]::ReadAllBytes($ownerBackbufferPath + '.tmp.png'), $beforeProbeTempBytes)) `
            'rejected second owner changed retained backbuffer fixture bytes'

        [IO.File]::WriteAllText($ownerReleasePath, 'exit', [Text.UTF8Encoding]::new($false))
        $cleanupDeadline = [DateTime]::UtcNow.AddSeconds(15)
        do {
            $originalRestored = (Test-Path -LiteralPath $ownerIniPath -PathType Leaf) -and
                [Linq.Enumerable]::SequenceEqual[byte]([IO.File]::ReadAllBytes($ownerIniPath), $ownerOriginalBytes)
            $backupLeft = @(Get-ChildItem -LiteralPath $ownerRuntime -Filter '.imgui.ini.readme-capture-*' -File).Count -gt 0
            $lockLeft = Test-Path -LiteralPath (Join-Path $ownerRuntime '.readme-capture.lock') -PathType Leaf
            $backbufferLeft = (Test-Path -LiteralPath $ownerBackbufferPath) -or
                (Test-Path -LiteralPath ($ownerBackbufferPath + '.tmp.png'))
            if ($originalRestored -and -not $backupLeft -and -not $lockLeft -and -not $backbufferLeft) { break }
            Start-Sleep -Milliseconds 25
        } while ([DateTime]::UtcNow -lt $cleanupDeadline)
        Assert-True (Test-Path -LiteralPath $ownerShutdownPath -PathType Leaf) 'retained child did not reach its shutdown write'
        Assert-True $originalRestored 'detached owner did not restore original imgui.ini bytes after child exit'
        Assert-True (-not $backupLeft) 'detached owner left an imgui backup or watcher-ready artifact'
        Assert-True (-not $lockLeft) 'detached owner left the runtime capture lock after restoration'
        Assert-True (-not $backbufferLeft) 'detached owner left the retained backbuffer or temporary sibling after child exit'

        $laterProbe = Invoke-TestPwshProcess -ScriptPath $probeOwnerScript -Arguments @(
            '-CaptureScript', $captureScript, '-RepoRoot', $repoRoot,
            '-RuntimeDir', $ownerRuntime, '-SuccessPath', $probeSuccessPath
        )
        Assert-True ($laterProbe.ExitCode -eq 0 -and (Test-Path -LiteralPath $probeSuccessPath -PathType Leaf)) `
            "later independent owner could not acquire the released runtime lock: $($laterProbe.Output)"
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $ownerRuntime '.readme-capture.lock'))) `
            'later independent owner did not release its runtime lock'
    }
    finally {
        if ($null -ne $ownerChildId) {
            $cleanupOwnerChild = Get-Process -Id $ownerChildId -ErrorAction SilentlyContinue
            $cleanupOwnerChildIsExact = $null -ne $cleanupOwnerChild -and
                $cleanupOwnerChild.StartTime.ToUniversalTime().Ticks -eq $ownerChildStartTimeUtcTicks
            if ($cleanupOwnerChildIsExact) {
                [IO.File]::WriteAllText($ownerReleasePath, 'exit', [Text.UTF8Encoding]::new($false))
                [void]$cleanupOwnerChild.WaitForExit(10000)
            }
            if ($null -ne $cleanupOwnerChild) { $cleanupOwnerChild.Dispose() }
        }
    }

    # A successful retained capture must keep the user's layout quarantined until
    # that exact child exits. Exercise the real Invoke-ProjectCapture lifecycle
    # and a real child process; only the GUI-specific capture mechanics are
    # replaced because this suite does not own a tutorial window.
    $retainedRoot = Join-Path $tempRoot 'retained child paths'
    $retainedRuntime = Join-Path $retainedRoot 'runtime with spaces'
    $retainedMedia = Join-Path $retainedRoot 'media with spaces'
    New-Item -ItemType Directory -Path $retainedRuntime, $retainedMedia -Force | Out-Null
    $retainedIniPath = Join-Path $retainedRuntime 'imgui.ini'
    $retainedReleasePath = Join-Path $retainedRoot 'release child'
    $retainedShutdownWrittenPath = Join-Path $retainedRoot 'shutdown ini written'
    $retainedChildScript = Join-Path $retainedRoot 'retained child.ps1'
    $retainedOriginalBytes = [byte[]](0x00, 0x11, 0x22, 0x7F, 0x80, 0xFE, 0xFF)
    $retainedCaptureBytes = [byte[]](0x63, 0x61, 0x70, 0x74, 0x75, 0x72, 0x65)
    [IO.File]::WriteAllBytes($retainedIniPath, $retainedOriginalBytes)
    [IO.File]::WriteAllText((Join-Path $retainedRuntime 'fixture.exe'), 'fixture')
    @'
param([string]$ReleasePath, [string]$IniPath, [string]$ShutdownWrittenPath)
$ErrorActionPreference = 'Stop'
while (-not (Test-Path -LiteralPath $ReleasePath)) {
    Start-Sleep -Milliseconds 25
}
[IO.File]::WriteAllBytes($IniPath, [byte[]](0x63, 0x61, 0x70, 0x74, 0x75, 0x72, 0x65))
[IO.File]::WriteAllText($ShutdownWrittenPath, 'written')
Start-Sleep -Milliseconds 250
'@ | Set-Content -LiteralPath $retainedChildScript -Encoding UTF8

    $savedStartReadmeCaptureProcess = (Get-Item Function:\Start-ReadmeCaptureProcess).ScriptBlock
    $savedWaitMainWindow = (Get-Item Function:\Wait-MainWindow).ScriptBlock
    $savedPrepareCaptureWindow = (Get-Item Function:\Prepare-CaptureWindow).ScriptBlock
    $savedCapturePreparedWindowPng = (Get-Item Function:\Capture-PreparedWindowPng).ScriptBlock
    $savedRestoreCaptureWindow = (Get-Item Function:\Restore-CaptureWindow).ScriptBlock
    $retainedChildId = $null
    $retainedChildStartTimeUtcTicks = $null
    try {
        function Start-ReadmeCaptureProcess {
            param(
                [string]$FilePath,
                [string]$WorkingDirectory,
                [switch]$EnableReadmeCapture,
                [string]$BackbufferPath,
                [int]$StillDelayMs,
                [scriptblock]$ProcessLauncher
            )

            $startInfo = [Diagnostics.ProcessStartInfo]::new()
            $startInfo.FileName = (Get-Process -Id $PID).Path
            $startInfo.UseShellExecute = $false
            $startInfo.CreateNoWindow = $true
            [void]$startInfo.ArgumentList.Add('-NoProfile')
            [void]$startInfo.ArgumentList.Add('-File')
            [void]$startInfo.ArgumentList.Add($script:retainedChildScript)
            [void]$startInfo.ArgumentList.Add('-ReleasePath')
            [void]$startInfo.ArgumentList.Add($script:retainedReleasePath)
            [void]$startInfo.ArgumentList.Add('-IniPath')
            [void]$startInfo.ArgumentList.Add($script:retainedIniPath)
            [void]$startInfo.ArgumentList.Add('-ShutdownWrittenPath')
            [void]$startInfo.ArgumentList.Add($script:retainedShutdownWrittenPath)
            $child = [Diagnostics.Process]::Start($startInfo)
            $script:retainedChildId = $child.Id
            $script:retainedChildStartTimeUtcTicks = $child.StartTime.ToUniversalTime().Ticks
            return $child
        }
        function Wait-MainWindow { param([Diagnostics.Process]$Process, [int]$TimeoutMs = 15000) }
        function Prepare-CaptureWindow {
            param([Diagnostics.Process]$Process, [int]$ClientWidth, [int]$ClientHeight, [string]$BackbufferPath)
            return [pscustomobject]@{ Handle = [IntPtr]::Zero }
        }
        function Capture-PreparedWindowPng {
            param([Diagnostics.Process]$Process, [object]$CaptureSession, [string]$OutputPath)
            New-TestPng -Path $OutputPath -Width 64 -Height 64
        }
        function Restore-CaptureWindow { param([object]$CaptureSession) }

        $retainedProject = [pscustomobject]@{
            number = '99'
            exe = 'fixture.exe'
            image = 'retained.png'
            gif = 'retained.gif'
            delayMs = 0
            gifPhase = 'runtime'
            gifPresentationPan = $false
            readmeBackbufferCapture = $false
            readmeCaptureMode = $false
            preCaptureActions = @()
        }
        $retainedManifest = [pscustomobject]@{ captureWidth = 64; captureHeight = 64 }
        $retainedRuntimeLock = Enter-ReadmeCaptureRuntimeLock -RuntimeDir $retainedRuntime
        try {
            $null = Invoke-ProjectCapture -Project $retainedProject -ManifestData $retainedManifest `
                -RuntimeDir $retainedRuntime -MediaDir $retainedMedia -RepoRoot $retainedRoot `
                -Attempt 1 -SkipGif -KeepWindows -RuntimeLockState $retainedRuntimeLock
        }
        finally {
            Exit-ReadmeCaptureRuntimeLock -State $retainedRuntimeLock
        }

        $retainedChildAfterCapture = Get-Process -Id $retainedChildId -ErrorAction SilentlyContinue
        $retainedChildIsExact = $null -ne $retainedChildAfterCapture -and
            $retainedChildAfterCapture.StartTime.ToUniversalTime().Ticks -eq $retainedChildStartTimeUtcTicks
        Assert-True $retainedChildIsExact `
            'successful KeepWindows capture did not retain its child process'
        if ($null -ne $retainedChildAfterCapture) { $retainedChildAfterCapture.Dispose() }
        $originalExposedEarly = (Test-Path -LiteralPath $retainedIniPath -PathType Leaf) -and
            [Linq.Enumerable]::SequenceEqual[byte]([IO.File]::ReadAllBytes($retainedIniPath), $retainedOriginalBytes)
        Assert-True (-not $originalExposedEarly) `
            'successful KeepWindows capture exposed the user imgui.ini before the retained child exited'

        Set-Content -LiteralPath $retainedReleasePath -Value 'exit' -NoNewline
        $shutdownWriteDeadline = [DateTime]::UtcNow.AddSeconds(10)
        while (-not (Test-Path -LiteralPath $retainedShutdownWrittenPath -PathType Leaf) -and
            [DateTime]::UtcNow -lt $shutdownWriteDeadline) {
            Start-Sleep -Milliseconds 25
        }
        $captureIniWrittenOnShutdown = (Test-Path -LiteralPath $retainedIniPath -PathType Leaf) -and
            [Linq.Enumerable]::SequenceEqual[byte]([IO.File]::ReadAllBytes($retainedIniPath), $retainedCaptureBytes)
        Assert-True $captureIniWrittenOnShutdown `
            'retained child did not write its capture imgui.ini during shutdown'
        $childExitDeadline = [DateTime]::UtcNow.AddSeconds(10)
        do {
            $childAfterRelease = Get-Process -Id $retainedChildId -ErrorAction SilentlyContinue
            $retainedChildExited = $null -eq $childAfterRelease -or
                $childAfterRelease.StartTime.ToUniversalTime().Ticks -ne $retainedChildStartTimeUtcTicks
            if ($null -ne $childAfterRelease) { $childAfterRelease.Dispose() }
            if ($retainedChildExited) { break }
            Start-Sleep -Milliseconds 25
        } while ([DateTime]::UtcNow -lt $childExitDeadline)
        Assert-True $retainedChildExited 'retained capture child did not exit after release'
        $restoreDeadline = [DateTime]::UtcNow.AddSeconds(10)
        do {
            $backupPaths = @(Get-ChildItem -LiteralPath $retainedRuntime -Filter '.imgui.ini.readme-capture-*.bak' -File)
            $originalRestored = (Test-Path -LiteralPath $retainedIniPath -PathType Leaf) -and
                [Linq.Enumerable]::SequenceEqual[byte]([IO.File]::ReadAllBytes($retainedIniPath), $retainedOriginalBytes)
            if ($originalRestored -and $backupPaths.Count -eq 0) { break }
            Start-Sleep -Milliseconds 25
        } while ([DateTime]::UtcNow -lt $restoreDeadline)
        Assert-True $originalRestored 'retained child shutdown did not restore the user imgui.ini byte-for-byte'
        Assert-True ($backupPaths.Count -eq 0) 'retained child shutdown left the recoverable imgui.ini backup behind'
    }
    finally {
        Set-Item Function:\Start-ReadmeCaptureProcess -Value $savedStartReadmeCaptureProcess
        Set-Item Function:\Wait-MainWindow -Value $savedWaitMainWindow
        Set-Item Function:\Prepare-CaptureWindow -Value $savedPrepareCaptureWindow
        Set-Item Function:\Capture-PreparedWindowPng -Value $savedCapturePreparedWindowPng
        Set-Item Function:\Restore-CaptureWindow -Value $savedRestoreCaptureWindow
        if ($null -ne $retainedChildId) {
            $cleanupChild = Get-Process -Id $retainedChildId -ErrorAction SilentlyContinue
            $cleanupChildIsExact = $null -ne $cleanupChild -and
                $cleanupChild.StartTime.ToUniversalTime().Ticks -eq $retainedChildStartTimeUtcTicks
            if ($cleanupChildIsExact) {
                Set-Content -LiteralPath $retainedReleasePath -Value 'exit' -NoNewline
                [void]$cleanupChild.WaitForExit(10000)
            }
            if ($null -ne $cleanupChild) { $cleanupChild.Dispose() }
        }
    }

    $previousCapture = $env:DX11_README_CAPTURE
    $previousBackbuffer = $env:DX11_README_BACKBUFFER_PNG
    $previousStillDelay = $env:DX11_README_STILL_DELAY_MS
    try {
        $env:DX11_README_CAPTURE = 'prior-capture'
        $env:DX11_README_BACKBUFFER_PNG = 'prior-backbuffer'
        $env:DX11_README_STILL_DELAY_MS = 'prior-delay'
        $observedLaunchEnvironment = $null
        $launcher = {
            param([string]$FilePath, [string]$WorkingDirectory)
            $script:observedLaunchEnvironment = [pscustomobject]@{
                Capture = $env:DX11_README_CAPTURE
                Backbuffer = $env:DX11_README_BACKBUFFER_PNG
                StillDelay = $env:DX11_README_STILL_DELAY_MS
            }
            return [pscustomobject]@{ FilePath = $FilePath; WorkingDirectory = $WorkingDirectory }
        }
        $launched = Start-ReadmeCaptureProcess -FilePath 'fixture.exe' -WorkingDirectory $tempRuntime `
            -EnableReadmeCapture -BackbufferPath 'fixture-backbuffer.png' -StillDelayMs 2500 -ProcessLauncher $launcher
        Assert-True ($observedLaunchEnvironment.Capture -ceq '1') 'capture child did not inherit README capture mode'
        Assert-True ($observedLaunchEnvironment.Backbuffer -ceq 'fixture-backbuffer.png') 'capture child did not inherit its backbuffer path'
        Assert-True ($observedLaunchEnvironment.StillDelay -ceq '2500') 'capture child did not inherit the manifest-derived still delay'
        Assert-True ($env:DX11_README_CAPTURE -ceq 'prior-capture' -and
            $env:DX11_README_BACKBUFFER_PNG -ceq 'prior-backbuffer' -and
            $env:DX11_README_STILL_DELAY_MS -ceq 'prior-delay') `
            'successful child launch did not restore prior capture environment state'

        $launchFailed = $false
        try {
            $null = Start-ReadmeCaptureProcess -FilePath 'fixture.exe' -WorkingDirectory $tempRuntime `
                -EnableReadmeCapture -StillDelayMs 3000 -ProcessLauncher { throw 'expected launch failure' }
        }
        catch { $launchFailed = $_.Exception.Message -match 'expected launch failure' }
        Assert-True $launchFailed 'failing child launcher did not surface its error'
        Assert-True ($env:DX11_README_CAPTURE -ceq 'prior-capture' -and
            $env:DX11_README_BACKBUFFER_PNG -ceq 'prior-backbuffer' -and
            $env:DX11_README_STILL_DELAY_MS -ceq 'prior-delay') `
            'failed child launch did not restore prior capture environment state'
    }
    finally {
        if ($null -eq $previousCapture) { Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue } else { $env:DX11_README_CAPTURE = $previousCapture }
        if ($null -eq $previousBackbuffer) { Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue } else { $env:DX11_README_BACKBUFFER_PNG = $previousBackbuffer }
        if ($null -eq $previousStillDelay) { Remove-Item Env:\DX11_README_STILL_DELAY_MS -ErrorAction SilentlyContinue } else { $env:DX11_README_STILL_DELAY_MS = $previousStillDelay }
    }

    # ---------------------------------------------------------------------------
    # Backbuffer PNG provider. Project 36 publishes its own swap-chain frames, so
    # the capture tool has to consume a file another process is rewriting rather
    # than screen-scrape a window.
    # ---------------------------------------------------------------------------
    $source = Join-Path $tempRoot 'backbuffer.png'
    $destination = Join-Path $tempRoot 'copied.png'
    New-TestPng -Path $source -Width 1600 -Height 900
    Copy-ReadmeBackbufferPng -SourcePath $source -OutputPath $destination -Width 1600 -Height 900
    $details = Get-CaptureOutputDetails $destination 1600 900
    Assert-True ($details.Dimensions -eq '1600x900') 'backbuffer PNG provider changed dimensions'

    # The publisher keeps rewriting the source, so the provider must be able to
    # read it while another handle holds it open for writing.
    $sharedHandle = [System.IO.File]::Open($source, [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::ReadWrite)
    try {
        $sharedDestination = Join-Path $tempRoot 'copied-shared.png'
        Copy-ReadmeBackbufferPng -SourcePath $source -OutputPath $sharedDestination -Width 1600 -Height 900
        Assert-True ((Get-CaptureOutputDetails $sharedDestination 1600 900).Dimensions -eq '1600x900') `
            'backbuffer PNG provider could not read a source that is still open for writing'
    }
    finally {
        $sharedHandle.Dispose()
    }

    # ... and it must let go of the source before it returns, or the publisher's
    # next atomic replace would fail against the tool's own handle.
    $sourceReleased = $true
    try { Remove-Item -LiteralPath $source -Force }
    catch { $sourceReleased = $false }
    Assert-True $sourceReleased 'backbuffer PNG provider kept the published source open after returning'
    Assert-True ((Get-CaptureOutputDetails $destination 1600 900).Dimensions -eq '1600x900') `
        'backbuffer PNG output did not survive deleting the source it was copied from'

    # A source of the wrong size must fail loudly instead of publishing a
    # mis-sized README asset.
    $wrongSizeSource = Join-Path $tempRoot 'wrong-size.png'
    New-TestPng -Path $wrongSizeSource -Width 800 -Height 450
    $wrongSizeRejected = $false
    try {
        Copy-ReadmeBackbufferPng -SourcePath $wrongSizeSource -OutputPath (Join-Path $tempRoot 'rejected.png') `
            -Width 1600 -Height 900 -TimeoutSeconds 1
    }
    catch {
        $wrongSizeRejected = $_.Exception.Message -match '800x450'
    }
    Assert-True $wrongSizeRejected 'backbuffer PNG provider accepted a source with the wrong dimensions'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $tempRoot 'rejected.png'))) `
        'backbuffer PNG provider wrote an output for a rejected source'

    # A source that never appears must time out rather than hang the capture run.
    $missingRejected = $false
    $missingTimer = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        Copy-ReadmeBackbufferPng -SourcePath (Join-Path $tempRoot 'never-published.png') `
            -OutputPath (Join-Path $tempRoot 'never.png') -Width 1600 -Height 900 -TimeoutSeconds 1
    }
    catch {
        $missingRejected = $true
    }
    $missingTimer.Stop()
    Assert-True $missingRejected 'backbuffer PNG provider succeeded even though the source was never published'
    Assert-True ($missingTimer.Elapsed.TotalSeconds -lt 20) 'backbuffer PNG provider did not bound its retry window'

    # The generated backbuffer path is deleted only when it really sits inside the
    # chosen media directory: the tool must never remove a path it did not create.
    $mediaLike = Join-Path $tempRoot 'media'
    New-Item -ItemType Directory -Path $mediaLike -Force | Out-Null
    $containedPath = Join-Path $mediaLike 'backbuffer-36.png'
    New-TestPng -Path $containedPath -Width 32 -Height 32
    New-TestPng -Path ($containedPath + '.tmp.png') -Width 32 -Height 32
    Remove-ReadmeBackbufferFile -Path $containedPath -MediaDir $mediaLike
    Remove-ReadmeBackbufferFile -Path ($containedPath + '.tmp.png') -MediaDir $mediaLike
    Assert-True (-not (Test-Path -LiteralPath $containedPath)) 'contained backbuffer output was not removed'
    Assert-True (-not (Test-Path -LiteralPath ($containedPath + '.tmp.png'))) `
        'a publication temporary left in the media directory by a killed capture was not removed'
    Assert-True (@(Get-ChildItem -LiteralPath $mediaLike -File).Count -eq 0) `
        'backbuffer cleanup left files in the media directory'

    $strayPath = Join-Path $tempRoot 'stray.png'
    New-TestPng -Path $strayPath -Width 32 -Height 32
    $strayRefused = $false
    try { Remove-ReadmeBackbufferFile -Path $strayPath -MediaDir $mediaLike }
    catch { $strayRefused = $_.Exception.Message -match 'outside mediaDir' }
    Assert-True $strayRefused 'backbuffer cleanup accepted a path outside the media directory'
    Assert-True (Test-Path -LiteralPath $strayPath -PathType Leaf) 'backbuffer cleanup deleted a file it did not create'

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
