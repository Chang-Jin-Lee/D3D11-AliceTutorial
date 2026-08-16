$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$modelPath = Join-Path $repoRoot 'Dx11\Resource\fbx\Public\MyAlice\Player\SampleModel.glb'

$failures = [System.Collections.Generic.List[string]]::new()
function Assert-True([bool]$Condition, [string]$Message) {
    if ($Condition) { Write-Host "  ok   $Message" } else { $failures.Add($Message); Write-Host "  FAIL $Message" }
}

function Read-GlbJson([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 20) { throw "not a GLB: $Path" }
    if ([System.BitConverter]::ToUInt32($bytes, 0) -ne 0x46546C67) { throw "bad GLB magic: $Path" }
    $total = [System.BitConverter]::ToUInt32($bytes, 8)
    $offset = 12
    while ($offset -lt $total) {
        $len  = [System.BitConverter]::ToUInt32($bytes, $offset)
        $type = [System.BitConverter]::ToUInt32($bytes, $offset + 4)
        if ($type -eq 0x4E4F534A) {
            return [System.Text.Encoding]::UTF8.GetString($bytes, $offset + 8, $len) | ConvertFrom-Json
        }
        $pad = if ($len % 4) { 4 - ($len % 4) } else { 0 }
        $offset += 8 + $len + $pad
    }
    throw "no JSON chunk in $Path"
}

Assert-True (Test-Path -LiteralPath $modelPath) "player model exists: $modelPath"
$doc = Read-GlbJson $modelPath

$expected = [ordered]@{
    'VRM_1' = 11.875; 'VRM_2' = 7.333; 'VRM_3' = 11.750; 'VRM_4' = 9.667
    'VRM_5' = 9.375;  'VRM_6' = 7.583; 'VRM_7' = 11.583
}

$byName = @{}
foreach ($a in $doc.animations) { $byName[$a.name] = $a }

foreach ($name in $expected.Keys) {
    if (-not $byName.ContainsKey($name)) {
        Assert-True $false "clip '$name' is present in the player model"
        continue
    }
    $maxTime = 0.0
    foreach ($s in $byName[$name].samplers) {
        $acc = $doc.accessors[$s.input]
        if ($null -ne $acc.max -and $acc.max.Count -gt 0) {
            $maxTime = [Math]::Max($maxTime, [double]$acc.max[0])
        }
    }
    Assert-True ([Math]::Abs($maxTime - $expected[$name]) -le 0.05) `
        ("clip '$name' runs {0:F3}s (expected {1:F3}s)" -f $maxTime, $expected[$name])
}

Assert-True ($byName.ContainsKey('T-Pose')) 'T-Pose is present and will be skipped by the showcase'

# The showcase drives every character from these clips, so the core humanoid
# bones they target must exist on the enemy models too.
$targeted = [System.Collections.Generic.HashSet[string]]::new()
foreach ($name in $expected.Keys) {
    if (-not $byName.ContainsKey($name)) { continue }
    foreach ($c in $byName[$name].channels) {
        $n = $doc.nodes[$c.target.node].name
        if ($n -like 'J_Bip*') { [void]$targeted.Add($n) }
    }
}
Assert-True ($targeted.Count -ge 50) "VRM clips drive $($targeted.Count) core humanoid bones"

foreach ($enemy in @('AliceEnemy1.glb', 'AliceEnemy2.glb', 'AliceEnemy3.glb')) {
    $enemyDoc = Read-GlbJson (Join-Path $repoRoot "Dx11\Resource\fbx\Public\MyAlice\Enemy\$enemy")
    $have = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($n in $enemyDoc.nodes) { [void]$have.Add($n.name) }
    $missing = @($targeted | Where-Object { -not $have.Contains($_) })
    Assert-True ($missing.Count -eq 0) "$enemy carries every core humanoid bone the VRM clips drive"
}

if ($failures.Count -gt 0) {
    Write-Host "Project 36 VRM clip inventory FAILED ($($failures.Count)):"
    foreach ($f in $failures) { Write-Host " - $f" }
    exit 1
}
Write-Host 'project 36 VRM clip inventory passed'
