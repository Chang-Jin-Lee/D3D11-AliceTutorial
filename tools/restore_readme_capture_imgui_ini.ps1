[CmdletBinding()]
param(
    [Parameter(Mandatory)][int]$ProcessId,
    [Parameter(Mandatory)][long]$ProcessStartTimeUtcTicks,
    [Parameter(Mandatory)][string]$IniPath,
    [Parameter(Mandatory)][string]$BackupPath,
    [Parameter(Mandatory)][string]$ReadyPath,
    [Parameter(Mandatory)][string]$LockPath,
    [Parameter(Mandatory)][string]$LockToken,
    [string]$BackbufferPath,
    [string]$MediaDir,
    [switch]$OriginalExisted
)

$ErrorActionPreference = 'Stop'

function Restore-RetainedCaptureImGuiIni {
    if (Test-Path -LiteralPath $IniPath) {
        Remove-Item -LiteralPath $IniPath -Force
    }

    if ($OriginalExisted) {
        if (-not (Test-Path -LiteralPath $BackupPath -PathType Leaf)) {
            throw "README capture cannot restore the user's imgui.ini because its backup is missing: $BackupPath"
        }
        [System.IO.File]::Move($BackupPath, $IniPath)
    }
}

function Get-RetainedBackbufferCleanupPaths {
    if ([string]::IsNullOrWhiteSpace($BackbufferPath)) {
        return @()
    }
    if ([string]::IsNullOrWhiteSpace($MediaDir) -or -not (Test-Path -LiteralPath $MediaDir -PathType Container)) {
        throw 'Retained backbuffer cleanup requires an existing media directory.'
    }

    $mediaFullPath = [IO.Path]::GetFullPath($MediaDir).TrimEnd('\', '/')
    $backbufferFullPath = [IO.Path]::GetFullPath($BackbufferPath)
    $mediaPrefix = $mediaFullPath + [IO.Path]::DirectorySeparatorChar
    if (-not $backbufferFullPath.StartsWith($mediaPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing retained backbuffer cleanup outside mediaDir: $backbufferFullPath"
    }
    if ([IO.Path]::GetFileName($backbufferFullPath) -notmatch '^backbuffer-\d{2}-[0-9a-f]{32}\.png$') {
        throw "Refusing retained backbuffer cleanup for an unexpected filename: $backbufferFullPath"
    }

    return @($backbufferFullPath, ($backbufferFullPath + '.tmp.png'))
}

function Exit-RetainedCaptureRuntimeLock {
    if (-not (Test-Path -LiteralPath $LockPath -PathType Leaf)) {
        throw "README capture runtime lock disappeared before deferred restoration completed: $LockPath"
    }
    $observedToken = [IO.File]::ReadAllText($LockPath, [Text.UTF8Encoding]::new($false))
    if ($observedToken -cne $LockToken) {
        throw "Refusing to release a README capture runtime lock owned by another acquisition: $LockPath"
    }
    [IO.File]::Delete($LockPath)
}

$targetProcess = $null
try {
    $expectedLockPath = Join-Path (Split-Path -Parent ([IO.Path]::GetFullPath($IniPath))) '.readme-capture.lock'
    if ([IO.Path]::GetFullPath($LockPath) -cne [IO.Path]::GetFullPath($expectedLockPath)) {
        throw "README capture runtime lock path does not match imgui.ini runtime: $LockPath"
    }
    if (-not (Test-Path -LiteralPath $LockPath -PathType Leaf) -or
        [IO.File]::ReadAllText($LockPath, [Text.UTF8Encoding]::new($false)) -cne $LockToken) {
        throw 'README capture runtime lock ownership changed before watcher acceptance.'
    }
    $cleanupPaths = @(Get-RetainedBackbufferCleanupPaths)

    try {
        $candidate = Get-Process -Id $ProcessId -ErrorAction Stop
    }
    catch {
        # The retained process can exit before this watcher opens it. In that
        # case there is nothing left that can rewrite imgui.ini, so restoration
        # is safe immediately.
        $candidate = $null
    }

    if ($null -ne $candidate) {
        try {
            $candidateStartTimeUtcTicks = $candidate.StartTime.ToUniversalTime().Ticks
        }
        catch {
            $candidate.Refresh()
            if (-not $candidate.HasExited) {
                throw
            }
            $candidateStartTimeUtcTicks = $null
        }

        if ($candidateStartTimeUtcTicks -eq $ProcessStartTimeUtcTicks) {
            $targetProcess = $candidate
        }
        else {
            # The exact child has exited if the PID now names a process with a
            # different start time; never wait on a reused PID.
            $candidate.Dispose()
        }
    }

    # Signal only after process identity and all paths have been accepted. The
    # capture script does not return from -KeepWindows until this handoff exists.
    [System.IO.File]::WriteAllText($ReadyPath, 'ready', [System.Text.UTF8Encoding]::new($false))

    if ($null -ne $targetProcess) {
        $targetProcess.WaitForExit()
    }

    Restore-RetainedCaptureImGuiIni
    foreach ($cleanupPath in $cleanupPaths) {
        if (Test-Path -LiteralPath $cleanupPath -PathType Leaf) {
            [IO.File]::Delete($cleanupPath)
        }
    }
    Exit-RetainedCaptureRuntimeLock
}
finally {
    if ($null -ne $targetProcess) {
        $targetProcess.Dispose()
    }
}
