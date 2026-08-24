[CmdletBinding()]
param(
    [Parameter(Mandatory)][int]$ProcessId,
    [Parameter(Mandatory)][long]$ProcessStartTimeUtcTicks,
    [Parameter(Mandatory)][string]$IniPath,
    [Parameter(Mandatory)][string]$BackupPath,
    [Parameter(Mandatory)][string]$ReadyPath,
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

$targetProcess = $null
try {
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
}
finally {
    if ($null -ne $targetProcess) {
        $targetProcess.Dispose()
    }
}
