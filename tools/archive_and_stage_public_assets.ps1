param(
    [switch]$Execute,
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$ArchiveRoot = 'C:\Users\k2503200021\Desktop\애셋\D3D11-AliceTutorial-2026-07-05',
    [string]$PlayerSource = 'C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\Player',
    [string]$EnemySource = 'C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\Enemy',
    [string]$AnimSource = 'C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\UEExportAnimOnly'
)

$ErrorActionPreference = 'Stop'

function Resolve-OrCreateDirectory {
    param(
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        return (Resolve-Path -LiteralPath $Path).Path
    }

    if ($Execute) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
        return (Resolve-Path -LiteralPath $Path).Path
    }

    return $Path
}

function Assert-UnderRoot {
    param(
        [string]$Target,
        [string]$Root,
        [string]$Label
    )

    $targetFull = [System.IO.Path]::GetFullPath($Target)
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    )

    if ($targetFull.Equals($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $targetFull
    }

    $rootPrefix = $rootFull + [System.IO.Path]::DirectorySeparatorChar
    if (-not $targetFull.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label path escaped root: $targetFull (root: $rootFull)"
    }

    return $targetFull
}

function Move-ToArchive {
    param(
        [string]$RelativePath
    )

    $source = Assert-UnderRoot -Target (Join-Path $RepoRoot $RelativePath) -Root $RepoRoot -Label 'Archive source'
    if (-not (Test-Path -LiteralPath $source)) {
        Write-Host "[skip] missing $RelativePath"
        return
    }

    $destination = Assert-UnderRoot -Target (Join-Path $ArchiveRoot $RelativePath) -Root $ArchiveRoot -Label 'Archive destination'
    $destinationParent = Split-Path -Parent $destination
    Resolve-OrCreateDirectory -Path $destinationParent | Out-Null

    Write-Host "[archive] $RelativePath -> $destination"
    if ($Execute) {
        Move-Item -LiteralPath $source -Destination $destination -Force
    }
}

function Copy-Asset {
    param(
        [string]$Source,
        [string]$RelativeDestination
    )

    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Missing source asset: $Source"
    }

    $destination = Assert-UnderRoot -Target (Join-Path $RepoRoot $RelativeDestination) -Root $RepoRoot -Label 'Repo destination'
    $destinationParent = Split-Path -Parent $destination
    Resolve-OrCreateDirectory -Path $destinationParent | Out-Null

    Write-Host "[copy] $Source -> $RelativeDestination"
    if ($Execute) {
        Copy-Item -LiteralPath $Source -Destination $destination -Force
    }
}

$RepoRoot = Resolve-OrCreateDirectory -Path $RepoRoot
$ArchiveRoot = Resolve-OrCreateDirectory -Path $ArchiveRoot

$restrictedPaths = @(
    'Dx11\Resource\fbx\Alice.fbx',
    'Dx11\Resource\fbx\Alice_UmaUma.fbx',
    'Dx11\Resource\fbx\Anis.fbx',
    'Dx11\Resource\fbx\Neon.fbx',
    'Dx11\Resource\fbx\Rapi.fbx',
    'Dx11\Resource\fbx\Study\Alice.fbm',
    'Dx11\Resource\fbx\Study\Alice_.fbm',
    'Dx11\Resource\fbx\Study\Alice_.fbx',
    'Dx11\Resource\fbx\Study\Alice3DGame',
    'Dx11\Resource\fbx\Study\Alice_Relative.fbx',
    'Dx11\Resource\fbx\Study\alice_normal_mapping.fbm',
    'Dx11\Resource\fbx\Study\alice_normal_mapping.fbx',
    'Dx11\Resource\fbx\Study\alice_normal_mapping_idle_walk_run.fbm',
    'Dx11\Resource\fbx\Study\alice_normal_mapping_idle_walk_run.fbx',
    'Dx11\Resource\fbx\Study\alice_rabbit.fbx',
    'Dx11\Resource\fbx\Study\alice_test.fbx',
    'Dx11\Resource\pmx\Nikke-Alice',
    'Dx11\Resource\Image\AliceDagwa.png',
    'Dx11\Resource\Image\AliceDagwaDone.png',
    'Dx11\Resource\Image\Manga',
    'Dx11\Resource\Live2D\Doro',
    'Dx11\Resource\Sound',
    'Dx11\Resource\sound'
)

foreach ($relativePath in $restrictedPaths) {
    Move-ToArchive -RelativePath $relativePath
}

Copy-Asset -Source (Join-Path $PlayerSource 'SampleModel.glb') -RelativeDestination 'Dx11\Resource\fbx\Public\MyAlice\Player\SampleModel.glb'
Copy-Asset -Source (Join-Path $EnemySource '1\AliceEnemy1.glb') -RelativeDestination 'Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy1.glb'
Copy-Asset -Source (Join-Path $EnemySource '2\AliceEnemy2.glb') -RelativeDestination 'Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy2.glb'
Copy-Asset -Source (Join-Path $EnemySource '3\AliceEnemy3.glb') -RelativeDestination 'Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy3.glb'
Copy-Asset -Source (Join-Path $AnimSource 'anim_Idle.fbx') -RelativeDestination 'Dx11\Resource\fbx\Public\MyAlice\Animations\anim_Idle.fbx'
Copy-Asset -Source (Join-Path $AnimSource 'Walk_Loop_F_0_Seq.fbx') -RelativeDestination 'Dx11\Resource\fbx\Public\MyAlice\Animations\Walk_Loop_F_0_Seq.fbx'
Copy-Asset -Source (Join-Path $AnimSource 'Run_Combat_Loop_F_0_Seq.fbx') -RelativeDestination 'Dx11\Resource\fbx\Public\MyAlice\Animations\Run_Combat_Loop_F_0_Seq.fbx'
Copy-Asset -Source (Join-Path $AnimSource 'Roll_F_0_Seq.fbx') -RelativeDestination 'Dx11\Resource\fbx\Public\MyAlice\Animations\Roll_F_0_Seq.fbx'

if ($Execute) {
    Write-Host '[done] executed asset archive and staging'
} else {
    Write-Host '[dry-run] pass -Execute to move and copy files'
}
