function Resolve-ReadmeMediaPath {
    param(
        [Parameter(Mandatory)] [string] $RepoRoot,
        [Parameter(Mandatory)] [string] $Path
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Path))
}

function Get-ReadmeMediaManifest {
    param(
        [Parameter(Mandatory)] [string] $ManifestPath,
        [Parameter(Mandatory)] [string] $RepoRoot
    )

    $fullManifestPath = Resolve-ReadmeMediaPath -RepoRoot $RepoRoot -Path $ManifestPath
    if (-not (Test-Path -LiteralPath $fullManifestPath -PathType Leaf)) {
        throw "README media manifest not found: $fullManifestPath"
    }

    return Get-Content -LiteralPath $fullManifestPath -Raw | ConvertFrom-Json
}

function Get-ReadmeMediaProject {
    param(
        [Parameter(Mandatory)] [object] $Manifest,
        [Parameter(Mandatory)] [string] $Number
    )

    $project = @($Manifest.projects | Where-Object { $_.number -eq $Number }) | Select-Object -First 1
    if ($null -eq $project) {
        throw "README media project not found: $Number"
    }

    return $project
}

function Test-ReadmeMediaManifestProperty {
    param([object] $Object, [string] $Name)

    return $null -ne $Object -and $null -ne $Object.PSObject.Properties[$Name]
}

function Test-ReadmeMediaPositiveInteger {
    param([object] $Value)

    try {
        $number = [double]$Value
        return -not [double]::IsNaN($number) -and
            -not [double]::IsInfinity($number) -and
            [math]::Truncate($number) -eq $number -and
            $number -gt 0
    }
    catch {
        return $false
    }
}

function Test-ReadmeMediaNumber {
    param([object] $Value)

    try {
        $number = [double]$Value
        return -not [double]::IsNaN($number) -and -not [double]::IsInfinity($number)
    }
    catch {
        return $false
    }
}

function Test-ReadmeMediaAction {
    param(
        [object] $Action,
        [string] $Number,
        [System.Collections.Generic.List[string]] $Errors,
        [bool] $RequiresAtMs
    )

    if ($null -eq $Action -or -not (Test-ReadmeMediaManifestProperty -Object $Action -Name 'type')) {
        $null = $Errors.Add("malformed action: $Number")
        return
    }

    if ($RequiresAtMs) {
        if (-not (Test-ReadmeMediaManifestProperty -Object $Action -Name 'atMs') -or
            -not (Test-ReadmeMediaNumber -Value $Action.atMs) -or
            [double]$Action.atMs -lt 0) {
            $null = $Errors.Add("malformed action timing: $Number")
        }
    }

    $validGifPhases = @('startup', 'runtime')
    $validActionTypes = @('wait', 'click', 'keyDown', 'keyUp')
    if ($Action.type -notin $validActionTypes) {
        $null = $Errors.Add("unsupported action type: $Number")
        return
    }

    switch ($Action.type) {
        'wait' {
            if (-not (Test-ReadmeMediaManifestProperty -Object $Action -Name 'durationMs') -or
                -not (Test-ReadmeMediaNumber -Value $Action.durationMs)) {
                $null = $Errors.Add("malformed wait action: $Number")
            }
            elseif ([int]$Action.durationMs -lt 0) { $null = $Errors.Add("negative wait duration: $Number") }
        }
        'click' {
            if (-not (Test-ReadmeMediaManifestProperty -Object $Action -Name 'x') -or
                -not (Test-ReadmeMediaManifestProperty -Object $Action -Name 'y') -or
                -not (Test-ReadmeMediaNumber -Value $Action.x) -or
                -not (Test-ReadmeMediaNumber -Value $Action.y)) {
                $null = $Errors.Add("malformed click action: $Number")
            }
            elseif ([double]$Action.x -lt 0 -or [double]$Action.x -gt 1 -or
                [double]$Action.y -lt 0 -or [double]$Action.y -gt 1) {
                $null = $Errors.Add("click coordinates must be normalized: $Number")
            }
        }
        'keyDown' {
            if (-not (Test-ReadmeMediaManifestProperty -Object $Action -Name 'key')) {
                $null = $Errors.Add("malformed key action: $Number")
            }
            if ($Action.key -notmatch '^[WASD]$') { $null = $Errors.Add("unsupported key: $Number") }
        }
        'keyUp' {
            if (-not (Test-ReadmeMediaManifestProperty -Object $Action -Name 'key')) {
                $null = $Errors.Add("malformed key action: $Number")
            }
            if ($Action.key -notmatch '^[WASD]$') { $null = $Errors.Add("unsupported key: $Number") }
        }
    }
}

function Test-ReadmeMediaManifest {
    param(
        [Parameter(Mandatory)] [object] $Manifest,
        [Parameter(Mandatory)] [string] $RepoRoot
    )

    $errors = [System.Collections.Generic.List[string]]::new()
    $validGifPhases = @('startup', 'runtime')
    $projectRoot = $RepoRoot
    if (Test-ReadmeMediaManifestProperty -Object $Manifest -Name 'runtimeDir') {
        $projectRoot = Split-Path -Parent (Resolve-ReadmeMediaPath -RepoRoot $RepoRoot -Path $Manifest.runtimeDir)
    }

    foreach ($property in @('expectedProjectCount', 'captureWidth', 'captureHeight', 'gifWidth', 'gifHeight', 'infoWidth', 'infoHeight', 'captureAttempts')) {
        if (-not (Test-ReadmeMediaManifestProperty -Object $Manifest -Name $property) -or
            -not (Test-ReadmeMediaPositiveInteger -Value $Manifest.$property)) {
            $null = $errors.Add("invalid manifest value: $property")
        }
    }

    foreach ($property in @('gifSeconds', 'gifFps', 'gifMaxBytes')) {
        if (-not (Test-ReadmeMediaManifestProperty -Object $Manifest -Name $property) -or
            -not (Test-ReadmeMediaPositiveInteger -Value $Manifest.$property)) {
            $null = $errors.Add("invalid manifest value: $property")
        }
    }

    $projects = @($Manifest.projects)
    if ($projects.Count -ne [int]$Manifest.expectedProjectCount) {
        $null = $errors.Add("project count mismatch: expected $($Manifest.expectedProjectCount), found $($projects.Count)")
    }

    foreach ($property in @('number', 'directory')) {
        foreach ($group in @($projects | Where-Object { Test-ReadmeMediaManifestProperty -Object $_ -Name $property } | Group-Object -Property $property)) {
            if ($group.Count -gt 1) {
                $null = $errors.Add("duplicate ${property}: $($group.Name)")
            }
        }
    }

    foreach ($project in $projects) {
        $number = if (Test-ReadmeMediaManifestProperty -Object $project -Name 'number') { [string]$project.number } else { '<unknown>' }
        foreach ($property in @('number', 'name', 'directory', 'exe', 'image', 'gif', 'infoImage', 'gifPhase', 'title', 'summary', 'tags')) {
            if (-not (Test-ReadmeMediaManifestProperty -Object $project -Name $property) -or
                $null -eq $project.$property -or
                ($project.$property -is [string] -and [string]::IsNullOrWhiteSpace($project.$property))) {
                $null = $errors.Add("missing metadata '$property': $number")
            }
        }

        if ($number -notmatch '^\d{2}$') {
            $null = $errors.Add("invalid project number: $number")
        }
        if (@($project.tags).Count -lt 3 -or @($project.tags).Count -gt 5) {
            $null = $errors.Add("invalid tags: $number")
        }
        if ($project.image -notmatch '\.png$') {
            $null = $errors.Add("invalid image output: $number")
        }
        if ($project.gif -notmatch '\.gif$') {
            $null = $errors.Add("invalid gif output: $number")
        }
        if ($project.infoImage -notmatch '^info/.+-info\.png$') {
            $null = $errors.Add("invalid info image output: $number")
        }
        if ($project.gifPhase -notin $validGifPhases) {
            $null = $errors.Add("unsupported gif phase: $number")
        }

        if (Test-ReadmeMediaManifestProperty -Object $project -Name 'directory') {
            $projectDirectory = Resolve-ReadmeMediaPath -RepoRoot $projectRoot -Path $project.directory
            if (-not (Test-Path -LiteralPath $projectDirectory -PathType Container)) {
                $null = $errors.Add("missing project directory: $number")
            }
            elseif (-not (Test-Path -LiteralPath (Join-Path $projectDirectory 'README.md') -PathType Leaf)) {
                $null = $errors.Add("missing project README: $number")
            }
        }

        if (Test-ReadmeMediaManifestProperty -Object $project -Name 'gifActions' -and $null -ne $project.gifActions) {
            foreach ($action in @($project.gifActions)) {
                Test-ReadmeMediaAction -Action $action -Number $number -Errors $errors -RequiresAtMs $true
            }
        }
        if (Test-ReadmeMediaManifestProperty -Object $project -Name 'preCaptureActions' -and $null -ne $project.preCaptureActions) {
            foreach ($action in @($project.preCaptureActions)) {
                Test-ReadmeMediaAction -Action $action -Number $number -Errors $errors -RequiresAtMs $false
            }
        }
    }

    return $errors.ToArray()
}
