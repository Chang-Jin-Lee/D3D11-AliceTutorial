# Public Character Asset Replacement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current NIKKE/Alice character, manga, Live2D, and sound runtime assets with user-authored VRoid character assets plus neutral project media, while wiring Unreal-exported `J_Bip` animation clips into the D3D11 animation controller.

**Architecture:** Keep the existing Assimp model path for GLB/FBX meshes. Add a small external animation clip library that owns Assimp importers for animation-only FBX files, then let `CharacterAnimController` resolve clips from that library before falling back to embedded animations. Use scripts for asset archiving/copying/generation so path checks are repeatable and old assets are preserved outside the repo.

**Tech Stack:** C++20, Direct3D 11, Assimp, FMOD, PowerShell 7/Windows PowerShell, Visual Studio MSBuild, UnrealEditor-Cmd 5.8 for local animation export.

---

## File Structure

- Create `C:\Github\D3D11-AliceTutorial\tools\archive_and_stage_public_assets.ps1`
  - Dry-runs and executes safe archive/copy operations for old restricted assets and new VRoid assets.
- Create `C:\Github\D3D11-AliceTutorial\tools\export_myalice_animations.py`
  - Unreal Python script that exports selected `AnimSequence` assets to local animation-only FBX files.
- Create `C:\Github\D3D11-AliceTutorial\tools\generate_neutral_media.ps1`
  - Generates neutral PNG and WAV resources used by the sample after old manga/sounds are archived.
- Create `C:\Github\D3D11-AliceTutorial\Dx11\Resource\ATTRIBUTION.md`
  - Documents `ChangJinLee` character permission and animation export status.
- Create `C:\Github\D3D11-AliceTutorial\Dx11\Common\Animation\ExternalAnimationClipLibrary.h`
  - Owns external animation imports and exposes key-based `aiAnimation*` lookup.
- Create `C:\Github\D3D11-AliceTutorial\Dx11\Common\Animation\ExternalAnimationClipLibrary.cpp`
  - Implements loading, lifetime ownership, and clip lookup.
- Modify `C:\Github\D3D11-AliceTutorial\Dx11\Common\Animation\CharacterAnimController.h`
  - Adds external clip fallback and an optional library parameter to `InitializeRig`.
- Modify `C:\Github\D3D11-AliceTutorial\Dx11\Common\Common.vcxproj`
  - Adds the new `.h/.cpp` files to the Common project.
- Modify `C:\Github\D3D11-AliceTutorial\Dx11\Common\Common.vcxproj.filters`
  - Adds the new files to the existing Animation filter.
- Modify `C:\Github\D3D11-AliceTutorial\Dx11\36_AdvancedAnim_Sound_Click\App_InternalTypes.inl`
  - Adds an `ExternalAnimationClipLibrary` member and updates manga/media comments.
- Modify `C:\Github\D3D11-AliceTutorial\Dx11\36_AdvancedAnim_Sound_Click\App_Lifecycle.inl`
  - Loads new models, neutral media, optional external clips, and initializes the controller with `J_Bip_R_Hand`.
- Modify `C:\Github\D3D11-AliceTutorial\Dx11\36_AdvancedAnim_Sound_Click\App_UpdateInput.inl`
  - Allows the advanced rig to run when no weapon model exists.
- Modify `C:\Github\D3D11-AliceTutorial\Dx11\36_AdvancedAnim_Sound_Click\App_ImGuiPanels.inl`
  - Updates loading/comic image references and manga page count.

## Asset Paths

Source assets already verified locally:

- Player source: `C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\Player\SampleModel.glb`
- Enemy 1 source: `C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\Enemy\1\AliceEnemy1.glb`
- Enemy 2 source: `C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\Enemy\2\AliceEnemy2.glb`
- Enemy 3 source: `C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\Enemy\3\AliceEnemy3.glb`
- Exported animation source: `C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\UEExportAnimOnly`

Runtime destination paths:

- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\fbx\Public\MyAlice\Player\SampleModel.glb`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy1.glb`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy2.glb`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy3.glb`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\fbx\Public\MyAlice\Animations\anim_Idle.fbx`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\fbx\Public\MyAlice\Animations\Walk_Loop_F_0_Seq.fbx`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\fbx\Public\MyAlice\Animations\Run_Combat_Loop_F_0_Seq.fbx`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\fbx\Public\MyAlice\Animations\Roll_F_0_Seq.fbx`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\Image\Public\Loading.png`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\Image\Public\LoadingDone.png`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\Image\Public\Comic\01.png`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\Image\Public\Comic\02.png`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\Image\Public\Comic\03.png`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\Sound\Public\ui_advance.wav`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\Sound\Public\ui_done.wav`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\Sound\Public\step.wav`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\Sound\Public\run.wav`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\Sound\Public\action.wav`
- `C:\Github\D3D11-AliceTutorial\Dx11\Resource\Sound\Public\reload.wav`

Old asset archive destination:

- Preferred: `C:\Users\k2503200021\Desktop\애셋\D3D11-AliceTutorial-2026-07-05`
- Fallback only if that path cannot be created: `C:\Users\k2503200021\Desktop\assets\D3D11-AliceTutorial-2026-07-05`

---

### Task 1: Add Asset Archive And Staging Script

**Files:**
- Create: `C:\Github\D3D11-AliceTutorial\tools\archive_and_stage_public_assets.ps1`

- [ ] **Step 1: Add the script in dry-run first**

Use `apply_patch` to create `tools\archive_and_stage_public_assets.ps1` with this complete content:

```powershell
param(
    [switch]$Execute,
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$ArchiveRoot = 'C:\Users\k2503200021\Desktop\애셋\D3D11-AliceTutorial-2026-07-05',
    [string]$PlayerSource = 'C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\Player',
    [string]$EnemySource = 'C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\Enemy',
    [string]$AnimSource = 'C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\UEExportAnimOnly'
)

$ErrorActionPreference = 'Stop'

function Resolve-OrCreateDirectory([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        if ($Execute) {
            New-Item -ItemType Directory -Path $Path -Force | Out-Null
        }
    }
    if (Test-Path -LiteralPath $Path) {
        return (Resolve-Path -LiteralPath $Path).Path
    }
    return $Path
}

function Assert-UnderRoot([string]$Target, [string]$Root, [string]$Label) {
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $targetFull = [System.IO.Path]::GetFullPath($Target)
    if (-not $targetFull.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label path escaped root. target=$targetFull root=$rootFull"
    }
}

function Move-ToArchive([string]$RelativePath) {
    $src = Join-Path $RepoRoot $RelativePath
    if (-not (Test-Path -LiteralPath $src)) {
        Write-Host "[skip] missing $RelativePath"
        return
    }
    $dst = Join-Path $ArchiveRoot $RelativePath
    Assert-UnderRoot $dst $ArchiveRoot 'archive'
    Write-Host "[archive] $src -> $dst"
    if ($Execute) {
        New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null
        Move-Item -LiteralPath $src -Destination $dst -Force
    }
}

function Copy-Asset([string]$Source, [string]$RelativeDestination) {
    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Missing source asset: $Source"
    }
    $dst = Join-Path $RepoRoot $RelativeDestination
    Assert-UnderRoot $dst $RepoRoot 'repo'
    Write-Host "[copy] $Source -> $dst"
    if ($Execute) {
        New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null
        Copy-Item -LiteralPath $Source -Destination $dst -Force
    }
}

$ArchiveRoot = Resolve-OrCreateDirectory $ArchiveRoot
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

foreach ($path in $restrictedPaths) {
    Move-ToArchive $path
}

Copy-Asset (Join-Path $PlayerSource 'SampleModel.glb') 'Dx11\Resource\fbx\Public\MyAlice\Player\SampleModel.glb'
Copy-Asset (Join-Path $EnemySource '1\AliceEnemy1.glb') 'Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy1.glb'
Copy-Asset (Join-Path $EnemySource '2\AliceEnemy2.glb') 'Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy2.glb'
Copy-Asset (Join-Path $EnemySource '3\AliceEnemy3.glb') 'Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy3.glb'

Copy-Asset (Join-Path $AnimSource 'anim_Idle.fbx') 'Dx11\Resource\fbx\Public\MyAlice\Animations\anim_Idle.fbx'
Copy-Asset (Join-Path $AnimSource 'Walk_Loop_F_0_Seq.fbx') 'Dx11\Resource\fbx\Public\MyAlice\Animations\Walk_Loop_F_0_Seq.fbx'
Copy-Asset (Join-Path $AnimSource 'Run_Combat_Loop_F_0_Seq.fbx') 'Dx11\Resource\fbx\Public\MyAlice\Animations\Run_Combat_Loop_F_0_Seq.fbx'
Copy-Asset (Join-Path $AnimSource 'Roll_F_0_Seq.fbx') 'Dx11\Resource\fbx\Public\MyAlice\Animations\Roll_F_0_Seq.fbx'

if ($Execute) {
    Write-Host '[done] executed asset archive and staging'
} else {
    Write-Host '[dry-run] pass -Execute to move and copy files'
}
```

- [ ] **Step 2: Run the dry-run**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File tools\archive_and_stage_public_assets.ps1
```

Expected: `[archive]` lines for existing restricted assets, `[copy]` lines for four character GLBs and four animation FBXs, and a final `[dry-run]` line.

- [ ] **Step 3: Execute archive/copy**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File tools\archive_and_stage_public_assets.ps1 -Execute
```

Expected: final line `[done] executed asset archive and staging`.

- [ ] **Step 4: Verify staged and archived assets**

Run:

```powershell
Test-Path 'Dx11\Resource\fbx\Public\MyAlice\Player\SampleModel.glb'
Test-Path 'Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy1.glb'
Test-Path 'Dx11\Resource\fbx\Public\MyAlice\Animations\anim_Idle.fbx'
Test-Path 'C:\Users\k2503200021\Desktop\애셋\D3D11-AliceTutorial-2026-07-05\Dx11\Resource\fbx\Study\Alice3DGame'
```

Expected: all four commands print `True`.

- [ ] **Step 5: Commit**

Run:

```powershell
git add tools\archive_and_stage_public_assets.ps1 Dx11\Resource\fbx\Public\MyAlice
git add -u Dx11\Resource
git commit -m "chore: stage public character assets"
```

Expected: commit succeeds and includes staged public GLB/FBX assets plus removals from old resource paths.

---

### Task 2: Add Attribution And Local Animation Export Script

**Files:**
- Create: `C:\Github\D3D11-AliceTutorial\Dx11\Resource\ATTRIBUTION.md`
- Create: `C:\Github\D3D11-AliceTutorial\tools\export_myalice_animations.py`

- [ ] **Step 1: Add attribution**

Create `Dx11\Resource\ATTRIBUTION.md`:

```markdown
# Resource Attribution

## Public MyAlice Characters

- Author: ChangJinLee
- Files:
  - `fbx/Public/MyAlice/Player/SampleModel.glb`
  - `fbx/Public/MyAlice/Enemy/AliceEnemy1.glb`
  - `fbx/Public/MyAlice/Enemy/AliceEnemy2.glb`
  - `fbx/Public/MyAlice/Enemy/AliceEnemy3.glb`
- Permission: ChangJinLee confirmed author ownership and approved these assets for this D3D11 tutorial project.
- Note: Source VRM metadata may retain authoring defaults. This project permission note is the controlling project-level record.

## MyAlice Animation Clips

- Source reference: `C:\Perforce_Main\AliceTopView_\Content\Assets\Characters\MyAlice`
- Export tool: `tools/export_myalice_animations.py`
- Runtime files:
  - `fbx/Public/MyAlice/Animations/anim_Idle.fbx`
  - `fbx/Public/MyAlice/Animations/Walk_Loop_F_0_Seq.fbx`
  - `fbx/Public/MyAlice/Animations/Run_Combat_Loop_F_0_Seq.fbx`
  - `fbx/Public/MyAlice/Animations/Roll_F_0_Seq.fbx`
- Redistribution status: local project export from the user's Unreal reference project. Keep source provenance visible before distributing these FBX files outside this repository.

## Neutral Project Media

- Files under `Image/Public` and `Sound/Public` are generated project media for this sample.
- They replace previous manga/loading/sound references.
```

- [ ] **Step 2: Add Unreal export script**

Create `tools\export_myalice_animations.py`:

```python
import pathlib
import unreal

OUTPUT_DIR = pathlib.Path(r"C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\UEExportAnimOnly")
ASSETS = [
    "/Game/Assets/Characters/MyAlice/Animation/anim_Idle",
    "/Game/Assets/Characters/MyAlice/Animation/Walk_Loop_F_0_Seq",
    "/Game/Assets/Characters/MyAlice/Animation/Run_Combat_Loop_F_0_Seq",
    "/Game/Assets/Characters/MyAlice/Animation/Roll_F_0_Seq",
]


def export_anim(asset_path: str) -> None:
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"Could not load {asset_path}")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    out_file = OUTPUT_DIR / f"{asset.get_name()}.fbx"

    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(out_file)
    task.automated = True
    task.replace_identical = True
    task.prompt = False

    options = unreal.FbxExportOption()
    options.ascii = False
    options.export_preview_mesh = False
    task.options = options

    ok = unreal.Exporter.run_asset_export_task(task)
    if not ok:
        raise RuntimeError(f"Export failed for {asset_path}")
    unreal.log(f"Exported {asset_path} -> {out_file}")


for path in ASSETS:
    export_anim(path)
```

- [ ] **Step 3: Run Unreal export only if FBX files are missing**

Run:

```powershell
$animDir = 'C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\UEExportAnimOnly'
if (-not (Test-Path "$animDir\anim_Idle.fbx")) {
  & 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
    'C:\Perforce_Main\AliceTopView_\AliceTopView.uproject' `
    -run=pythonscript `
    "-script=C:\Github\D3D11-AliceTutorial\tools\export_myalice_animations.py" `
    -unattended -nop4 -nosplash -NoSound
}
```

Expected: if files already exist, no export runs; otherwise Unreal logs four `Exported` lines.

- [ ] **Step 4: Commit**

Run:

```powershell
git add Dx11\Resource\ATTRIBUTION.md tools\export_myalice_animations.py
git commit -m "docs: document public asset provenance"
```

Expected: commit succeeds.

---

### Task 3: Add Neutral Media Generator

**Files:**
- Create: `C:\Github\D3D11-AliceTutorial\tools\generate_neutral_media.ps1`

- [ ] **Step 1: Add media generation script**

Create `tools\generate_neutral_media.ps1`:

```powershell
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

function New-Image([string]$Path, [string]$Title, [string]$Subtitle, [System.Drawing.Color]$BackColor) {
    $dir = Split-Path -Parent $Path
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $bmp = New-Object System.Drawing.Bitmap 1280, 720
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear($BackColor)
    $fontTitle = New-Object System.Drawing.Font 'Segoe UI', 56, ([System.Drawing.FontStyle]::Bold)
    $fontSub = New-Object System.Drawing.Font 'Segoe UI', 28, ([System.Drawing.FontStyle]::Regular)
    $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
    $g.DrawString($Title, $fontTitle, $brush, 80, 240)
    $g.DrawString($Subtitle, $fontSub, $brush, 86, 330)
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
}

function New-Wav([string]$Path, [int]$FrequencyHz, [double]$Seconds, [double]$Volume) {
    $dir = Split-Path -Parent $Path
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $sampleRate = 44100
    $samples = [int]($sampleRate * $Seconds)
    $dataBytes = $samples * 2
    $fs = [System.IO.File]::Create($Path)
    $bw = New-Object System.IO.BinaryWriter $fs
    $bw.Write([Text.Encoding]::ASCII.GetBytes('RIFF'))
    $bw.Write([int](36 + $dataBytes))
    $bw.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
    $bw.Write([int]16)
    $bw.Write([int16]1)
    $bw.Write([int16]1)
    $bw.Write([int]$sampleRate)
    $bw.Write([int]($sampleRate * 2))
    $bw.Write([int16]2)
    $bw.Write([int16]16)
    $bw.Write([Text.Encoding]::ASCII.GetBytes('data'))
    $bw.Write([int]$dataBytes)
    for ($i = 0; $i -lt $samples; $i++) {
        $t = $i / $sampleRate
        $env = [Math]::Min(1.0, $i / 500.0) * [Math]::Min(1.0, ($samples - $i) / 1000.0)
        $v = [Math]::Sin(2.0 * [Math]::PI * $FrequencyHz * $t) * $Volume * $env
        $bw.Write([int16]([Math]::Round($v * 32767)))
    }
    $bw.Dispose()
    $fs.Dispose()
}

$imageRoot = Join-Path $RepoRoot 'Dx11\Resource\Image\Public'
$soundRoot = Join-Path $RepoRoot 'Dx11\Resource\Sound\Public'

New-Image (Join-Path $imageRoot 'Loading.png') 'MyAlice D3D11' 'Loading public demo assets' ([System.Drawing.Color]::FromArgb(26, 32, 44))
New-Image (Join-Path $imageRoot 'LoadingDone.png') 'Ready' 'Click to start the public asset demo' ([System.Drawing.Color]::FromArgb(22, 64, 58))
New-Image (Join-Path $imageRoot 'Comic\01.png') 'Scene 01' 'Character asset replacement sample' ([System.Drawing.Color]::FromArgb(48, 52, 70))
New-Image (Join-Path $imageRoot 'Comic\02.png') 'Scene 02' 'External animation clip test' ([System.Drawing.Color]::FromArgb(54, 48, 70))
New-Image (Join-Path $imageRoot 'Comic\03.png') 'Scene 03' 'Neutral media path complete' ([System.Drawing.Color]::FromArgb(70, 55, 42))

New-Wav (Join-Path $soundRoot 'ui_advance.wav') 660 0.15 0.25
New-Wav (Join-Path $soundRoot 'ui_done.wav') 880 0.25 0.22
New-Wav (Join-Path $soundRoot 'step.wav') 220 0.08 0.18
New-Wav (Join-Path $soundRoot 'run.wav') 330 0.08 0.18
New-Wav (Join-Path $soundRoot 'action.wav') 740 0.12 0.20
New-Wav (Join-Path $soundRoot 'reload.wav') 520 0.18 0.20

Write-Host '[done] generated neutral media'
```

- [ ] **Step 2: Generate neutral media**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File tools\generate_neutral_media.ps1
```

Expected: `[done] generated neutral media`.

- [ ] **Step 3: Verify media exists**

Run:

```powershell
Test-Path 'Dx11\Resource\Image\Public\Loading.png'
Test-Path 'Dx11\Resource\Image\Public\Comic\03.png'
Test-Path 'Dx11\Resource\Sound\Public\ui_advance.wav'
```

Expected: all three commands print `True`.

- [ ] **Step 4: Commit**

Run:

```powershell
git add tools\generate_neutral_media.ps1 Dx11\Resource\Image\Public Dx11\Resource\Sound\Public
git commit -m "chore: add neutral sample media"
```

Expected: commit succeeds.

---

### Task 4: Add External Animation Clip Library

**Files:**
- Create: `C:\Github\D3D11-AliceTutorial\Dx11\Common\Animation\ExternalAnimationClipLibrary.h`
- Create: `C:\Github\D3D11-AliceTutorial\Dx11\Common\Animation\ExternalAnimationClipLibrary.cpp`
- Modify: `C:\Github\D3D11-AliceTutorial\Dx11\Common\Common.vcxproj`
- Modify: `C:\Github\D3D11-AliceTutorial\Dx11\Common\Common.vcxproj.filters`

- [ ] **Step 1: Create header**

Create `Dx11\Common\Animation\ExternalAnimationClipLibrary.h`:

```cpp
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct aiAnimation;
struct aiScene;

namespace Assimp
{
    class Importer;
}

class ExternalAnimationClipLibrary
{
public:
    bool LoadClip(const std::string& key, const std::wstring& pathW, std::string* errorOut = nullptr);
    void Clear();

    const aiAnimation* Get(const std::string& key) const;
    bool Has(const std::string& key) const;
    std::vector<std::string> Keys() const;

private:
    struct ClipEntry
    {
        std::wstring pathW;
        std::unique_ptr<Assimp::Importer> importer;
        const aiScene* scene = nullptr;
        unsigned animationIndex = 0;
    };

    std::unordered_map<std::string, ClipEntry> m_Clips;
};
```

- [ ] **Step 2: Create implementation**

Create `Dx11\Common\Animation\ExternalAnimationClipLibrary.cpp`:

```cpp
#include "pch.h"
#include "ExternalAnimationClipLibrary.h"

#include "../Helper.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

bool ExternalAnimationClipLibrary::LoadClip(const std::string& key, const std::wstring& pathW, std::string* errorOut)
{
    if (key.empty())
    {
        if (errorOut) *errorOut = "empty animation key";
        return false;
    }

    auto importer = std::make_unique<Assimp::Importer>();
    const unsigned flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_LimitBoneWeights |
        aiProcess_ValidateDataStructure;

    const std::string pathUtf8 = Utf8FromWString(pathW);
    const aiScene* scene = importer->ReadFile(pathUtf8, flags);
    if (!scene)
    {
        if (errorOut) *errorOut = importer->GetErrorString();
        return false;
    }
    if (scene->mNumAnimations == 0 || !scene->mAnimations || !scene->mAnimations[0])
    {
        if (errorOut) *errorOut = "file contains no animation";
        return false;
    }

    ClipEntry entry;
    entry.pathW = pathW;
    entry.importer = std::move(importer);
    entry.scene = scene;
    entry.animationIndex = 0;
    m_Clips[key] = std::move(entry);
    return true;
}

void ExternalAnimationClipLibrary::Clear()
{
    m_Clips.clear();
}

const aiAnimation* ExternalAnimationClipLibrary::Get(const std::string& key) const
{
    const auto it = m_Clips.find(key);
    if (it == m_Clips.end()) return nullptr;

    const ClipEntry& entry = it->second;
    if (!entry.scene || entry.animationIndex >= entry.scene->mNumAnimations) return nullptr;
    return entry.scene->mAnimations[entry.animationIndex];
}

bool ExternalAnimationClipLibrary::Has(const std::string& key) const
{
    return Get(key) != nullptr;
}

std::vector<std::string> ExternalAnimationClipLibrary::Keys() const
{
    std::vector<std::string> keys;
    keys.reserve(m_Clips.size());
    for (const auto& pair : m_Clips)
    {
        keys.push_back(pair.first);
    }
    return keys;
}
```

- [ ] **Step 3: Register files in Common project**

Modify `Dx11\Common\Common.vcxproj`:

```xml
<ClInclude Include="Animation\ExternalAnimationClipLibrary.h" />
```

Add it in the existing `<ItemGroup>` that contains `Animation\CharacterAnimController.h`.

```xml
<ClCompile Include="Animation\ExternalAnimationClipLibrary.cpp" />
```

Add it in the existing `<ItemGroup>` that contains `Animation\Animator.cpp`.

- [ ] **Step 4: Register filters**

Modify `Dx11\Common\Common.vcxproj.filters`:

```xml
<ClInclude Include="Animation\ExternalAnimationClipLibrary.h">
  <Filter>Header Files\Animation</Filter>
</ClInclude>
```

```xml
<ClCompile Include="Animation\ExternalAnimationClipLibrary.cpp">
  <Filter>Source Files\Animation</Filter>
</ClCompile>
```

Use the same filter names already used by `Animation\Animator.h` and `Animation\Animator.cpp`.

- [ ] **Step 5: Build Common**

Run:

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild 'Dx11\TutorialApp.sln' /t:Common /p:Configuration=Debug /p:Platform=x64
```

Expected: MSBuild finishes with `Build succeeded`.

- [ ] **Step 6: Commit**

Run:

```powershell
git add Dx11\Common\Animation\ExternalAnimationClipLibrary.h Dx11\Common\Animation\ExternalAnimationClipLibrary.cpp Dx11\Common\Common.vcxproj Dx11\Common\Common.vcxproj.filters
git commit -m "feat: add external animation clip library"
```

Expected: commit succeeds.

---

### Task 5: Wire External Clips Into CharacterAnimController

**Files:**
- Modify: `C:\Github\D3D11-AliceTutorial\Dx11\Common\Animation\CharacterAnimController.h`

- [ ] **Step 1: Include the external library**

Near the other local animation includes, add:

```cpp
#include "ExternalAnimationClipLibrary.h"
```

- [ ] **Step 2: Extend `AnimLibrary`**

Replace the current `AnimLibrary` fields and `Get` method with:

```cpp
struct AnimLibrary
{
    const aiScene* scene = nullptr;
    const std::unordered_map<std::string, int>* indexMap = nullptr;
    const ExternalAnimationClipLibrary* externalClips = nullptr;

    const aiAnimation* Get(const std::string& key) const
    {
        if (externalClips)
        {
            if (const aiAnimation* ext = externalClips->Get(key))
                return ext;
        }

        if (!scene || !indexMap) return nullptr;
        auto it = indexMap->find(key);
        if (it == indexMap->end()) return nullptr;
        const int idx = it->second;
        if (idx < 0 || (unsigned)idx >= scene->mNumAnimations) return nullptr;
        return scene->mAnimations[idx];
    }
```

Keep the existing `LengthSec`, `WrapOrClamp`, and `Norm01` methods unchanged after this block.

- [ ] **Step 3: Extend `InitializeRig`**

Change the signature from:

```cpp
const std::vector<std::string>* optionalAnimNames = nullptr)
```

to:

```cpp
const std::vector<std::string>* optionalAnimNames = nullptr,
const ExternalAnimationClipLibrary* externalClips = nullptr)
```

Then set the field directly after `m_Lib.indexMap = &animIndex;`:

```cpp
m_Lib.externalClips = externalClips;
```

- [ ] **Step 4: Build**

Run:

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild 'Dx11\TutorialApp.sln' /t:Common /p:Configuration=Debug /p:Platform=x64
```

Expected: MSBuild finishes with `Build succeeded`.

- [ ] **Step 5: Commit**

Run:

```powershell
git add Dx11\Common\Animation\CharacterAnimController.h
git commit -m "feat: allow external character animation clips"
```

Expected: commit succeeds.

---

### Task 6: Update Advanced Sample Runtime Paths

**Files:**
- Modify: `C:\Github\D3D11-AliceTutorial\Dx11\36_AdvancedAnim_Sound_Click\App_InternalTypes.inl`
- Modify: `C:\Github\D3D11-AliceTutorial\Dx11\36_AdvancedAnim_Sound_Click\App_Lifecycle.inl`
- Modify: `C:\Github\D3D11-AliceTutorial\Dx11\36_AdvancedAnim_Sound_Click\App_UpdateInput.inl`

- [ ] **Step 1: Add external clip member**

In `App_InternalTypes.inl`, near `CharacterAnimController m_CharCtrl;`, add:

```cpp
ExternalAnimationClipLibrary m_ExternalAnimClips;
```

Change the weapon index default:

```cpp
int m_WeaponModelIndex = -1;
```

Change the manga comment:

```cpp
int m_MangaIndex = 0;  // 0 = Public loading image, 1-3 = Public comic pages
```

- [ ] **Step 2: Load neutral sounds and images**

In `App_Lifecycle.inl`, replace the initial manga/sound setup block with:

```cpp
m_->m_MangaIndex = 0;
m_->m_CurrentSceneImagePath = L"..\\Resource\\Image\\Public\\Loading.png";
m_->m_OriginalSceneImagePath = m_->m_CurrentSceneImagePath;
m_->m_LoadingDoneSoundPlayed = false;

Sound::Load(L"UiAdvance", L"..\\Resource\\Sound\\Public\\ui_advance.wav", Sound::Type::SFX);
Sound::Load(L"UiDone", L"..\\Resource\\Sound\\Public\\ui_done.wav", Sound::Type::SFX);
Sound::Load(L"Walk", L"..\\Resource\\Sound\\Public\\step.wav", Sound::Type::SFX);
Sound::Load(L"RunVoice", L"..\\Resource\\Sound\\Public\\run.wav", Sound::Type::SFX);
Sound::Load(L"Shoot", L"..\\Resource\\Sound\\Public\\action.wav", Sound::Type::SFX);
Sound::Load(L"ShootCharged", L"..\\Resource\\Sound\\Public\\action.wav", Sound::Type::SFX);
Sound::Load(L"Reload", L"..\\Resource\\Sound\\Public\\reload.wav", Sound::Type::SFX);
```

Remove loads for `Dagwa`, `Waitforsecond`, `LoadingDone`, `LetItHappen`, `test`, `CaliforniaGirls`, `CaramellDansen`, `MeniShukiRushshu`, `RabbitHole`, and `Specialist`.

- [ ] **Step 3: Load replacement models**

Replace the old Alice model loading lines with:

```cpp
LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb"); // 0 player
LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Enemy\\AliceEnemy1.glb");  // 1 enemy
LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Enemy\\AliceEnemy2.glb");  // 2 enemy
LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Enemy\\AliceEnemy3.glb");  // 3 enemy

m_->m_CharModelIndex = 0;
m_->m_WeaponModelIndex = -1;
```

Remove loading of `Study\Alice3DGame\Alice_.fbx`, `AmazingWonderland.fbx`, and duplicate old Alice dance models.

- [ ] **Step 4: Load external animation clips**

Before controller initialization in `App_Lifecycle.inl`, add:

```cpp
auto LoadExternalClip = [&](const std::string& key, const std::wstring& path) {
    std::string err;
    if (m_->m_ExternalAnimClips.LoadClip(key, path, &err))
        m_->PushLog("[OK] ExternalAnim: " + key);
    else
        m_->PushLog("[WARN] ExternalAnim failed: " + key + " : " + err);
};

LoadExternalClip("Idle", L"..\\Resource\\fbx\\Public\\MyAlice\\Animations\\anim_Idle.fbx");
LoadExternalClip("Walk", L"..\\Resource\\fbx\\Public\\MyAlice\\Animations\\Walk_Loop_F_0_Seq.fbx");
LoadExternalClip("Run", L"..\\Resource\\fbx\\Public\\MyAlice\\Animations\\Run_Combat_Loop_F_0_Seq.fbx");
LoadExternalClip("Roll", L"..\\Resource\\fbx\\Public\\MyAlice\\Animations\\Roll_F_0_Seq.fbx");
```

- [ ] **Step 5: Update controller initialization**

Change comments and variable names from Alice-specific wording to player wording.

Set the socket to the new skeleton:

```cpp
m_->m_CharCtrl.config.weaponSocket.socketName = "WeaponPoint";
m_->m_CharCtrl.config.weaponSocket.parentBone = "J_Bip_R_Hand";
m_->m_CharCtrl.config.weaponSocket.pos = { 0.0f, 0.0f, 0.0f };
m_->m_CharCtrl.config.weaponSocket.rotDeg = { 0.0f, 0.0f, 0.0f };
m_->m_CharCtrl.config.weaponSocket.scale = { 1.0f, 1.0f, 1.0f };
```

Pass the external library into `InitializeRig`:

```cpp
&player.shared->fbx->GetAnimationNames(),
&m_->m_ExternalAnimClips))
```

- [ ] **Step 6: Make weapon optional in update**

In `App_UpdateInput.inl`, change the advanced rig guard from:

```cpp
if (m_->m_UseAdvancedRig && m_->m_CharRigInited && m_->m_Models.size() >= 2) {
```

to:

```cpp
if (m_->m_UseAdvancedRig && m_->m_CharRigInited && m_->m_Models.size() >= 1) {
```

Replace the invalid index block with:

```cpp
if (ci < 0 || ci >= (int)m_->m_Models.size()) {
    // invalid character index -> fallback to normal update path
}
else {
    auto& player = *m_->m_Models[(size_t)ci];
    ModelEntry* weapon = nullptr;
    if (wi >= 0 && wi < (int)m_->m_Models.size())
        weapon = m_->m_Models[(size_t)wi].get();
```

Then change the `TickAndApply` call to:

```cpp
m_->m_CharCtrl.TickAndApply(dt, input, player, weapon, m_->m_pDevice, m_->m_pDeviceContext,
    m_->m_TpsCamAttached ? &aim : nullptr);
```

- [ ] **Step 7: Remove old sound boxes**

Delete the blocks that create sound boxes for old BGM keys: `CaramellDansen`, `LetItHappen`, `CaliforniaGirls`, `MeniShukiRushshu`, `RabbitHole`, and `Specialist`.

- [ ] **Step 8: Build sample**

Run:

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild 'Dx11\TutorialApp.sln' /t:36_AdvancedAnim_Sound_Click /p:Configuration=Debug /p:Platform=x64
```

Expected: MSBuild finishes with `Build succeeded`.

- [ ] **Step 9: Commit**

Run:

```powershell
git add Dx11\36_AdvancedAnim_Sound_Click\App_InternalTypes.inl Dx11\36_AdvancedAnim_Sound_Click\App_Lifecycle.inl Dx11\36_AdvancedAnim_Sound_Click\App_UpdateInput.inl
git commit -m "feat: load public characters in advanced sample"
```

Expected: commit succeeds.

---

### Task 7: Update ImGui Loading And Comic Viewer Paths

**Files:**
- Modify: `C:\Github\D3D11-AliceTutorial\Dx11\36_AdvancedAnim_Sound_Click\App_ImGuiPanels.inl`

- [ ] **Step 1: Replace loading done behavior**

Change the loading completion block from old `LoadingDone`/`AliceDagwaDone` paths to:

```cpp
if (!m_->m_LoadingDoneSoundPlayed)
{
    Sound::PlaySFX(L"UiDone", 1.0f, 1.0f, false);
    m_->m_LoadingDoneSoundPlayed = true;
    m_->m_CurrentSceneImagePath = L"..\\Resource\\Image\\Public\\LoadingDone.png";
    LoadSceneImage(m_->m_CurrentSceneImagePath);
}
```

- [ ] **Step 2: Replace restart/reset path**

Change any reset to old `AliceDagwa.png` to:

```cpp
m_->m_MangaIndex = 0;
m_->m_CurrentSceneImagePath = L"..\\Resource\\Image\\Public\\Loading.png";
```

- [ ] **Step 3: Replace comic click path**

Change the comic click progression to three public pages:

```cpp
if (m_->m_MangaIndex == 0)
{
    m_->m_MangaIndex = 1;
    m_->m_CurrentSceneImagePath = std::format(L"..\\Resource\\Image\\Public\\Comic\\{:02d}.png", m_->m_MangaIndex);
    LoadSceneImage(m_->m_CurrentSceneImagePath);
    Sound::PlaySFX(L"UiAdvance", 1.0f, 1.0f, false);
}
else if (m_->m_MangaIndex < 3)
{
    m_->m_MangaIndex++;
    m_->m_CurrentSceneImagePath = std::format(L"..\\Resource\\Image\\Public\\Comic\\{:02d}.png", m_->m_MangaIndex);
    LoadSceneImage(m_->m_CurrentSceneImagePath);
    Sound::PlaySFX(L"UiAdvance", 1.0f, 1.0f, false);
}
```

Remove branches that play `Waitforsecond`, `Waitforsecond2`, or `Waitforsecond3`.

- [ ] **Step 4: Search for old media references**

Run:

```powershell
rg -n "AliceDagwa|Manga\\\\|Manga/|Waitforsecond|LoadingDone|Dagwa" Dx11\36_AdvancedAnim_Sound_Click
```

Expected: no runtime references remain. Comments may remain only if they describe archived old assets; prefer removing those comments in touched blocks.

- [ ] **Step 5: Build sample**

Run:

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild 'Dx11\TutorialApp.sln' /t:36_AdvancedAnim_Sound_Click /p:Configuration=Debug /p:Platform=x64
```

Expected: MSBuild finishes with `Build succeeded`.

- [ ] **Step 6: Commit**

Run:

```powershell
git add Dx11\36_AdvancedAnim_Sound_Click\App_ImGuiPanels.inl
git commit -m "feat: replace manga viewer media paths"
```

Expected: commit succeeds.

---

### Task 8: Verify No Old Runtime References Remain

**Files:**
- Modify as needed only in files reported by the searches.

- [ ] **Step 1: Search old character references**

Run:

```powershell
rg -n "Alice3DGame|Alice_|Alice_UmaUma|AmazingWonderland|Anis|Neon|Rapi|Nikke-Alice|Doro" Dx11\36_AdvancedAnim_Sound_Click Dx11\Common
```

Expected: no runtime-loading references remain in `36_AdvancedAnim_Sound_Click`. Shared loaders may still contain generic PMX/Live2D-capable code.

- [ ] **Step 2: Search old media references**

Run:

```powershell
rg -n "CaramellDansen|CaliforniaGirls|MeniShukiRushshu|RabbitHole|Specialist|LetItHappen|Run_voice|Dagwa|Waitforsecond|LoadingDone" Dx11\36_AdvancedAnim_Sound_Click Dx11\Resource
```

Expected: no runtime references remain under `36_AdvancedAnim_Sound_Click`; archived files are outside the repo resource paths.

- [ ] **Step 3: Fix each reported runtime reference**

For each reported runtime-loading line in `Dx11\36_AdvancedAnim_Sound_Click`, replace it with one of these public keys/paths:

```cpp
Sound::Load(L"UiAdvance", L"..\\Resource\\Sound\\Public\\ui_advance.wav", Sound::Type::SFX);
Sound::Load(L"UiDone", L"..\\Resource\\Sound\\Public\\ui_done.wav", Sound::Type::SFX);
Sound::Load(L"Walk", L"..\\Resource\\Sound\\Public\\step.wav", Sound::Type::SFX);
Sound::Load(L"RunVoice", L"..\\Resource\\Sound\\Public\\run.wav", Sound::Type::SFX);
Sound::Load(L"Shoot", L"..\\Resource\\Sound\\Public\\action.wav", Sound::Type::SFX);
Sound::Load(L"ShootCharged", L"..\\Resource\\Sound\\Public\\action.wav", Sound::Type::SFX);
Sound::Load(L"Reload", L"..\\Resource\\Sound\\Public\\reload.wav", Sound::Type::SFX);
```

For old model paths, use:

```cpp
L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb"
L"..\\Resource\\fbx\\Public\\MyAlice\\Enemy\\AliceEnemy1.glb"
L"..\\Resource\\fbx\\Public\\MyAlice\\Enemy\\AliceEnemy2.glb"
L"..\\Resource\\fbx\\Public\\MyAlice\\Enemy\\AliceEnemy3.glb"
```

- [ ] **Step 4: Commit cleanup**

Run:

```powershell
git add Dx11\36_AdvancedAnim_Sound_Click Dx11\Common
git commit -m "chore: remove old asset runtime references"
```

Expected: commit succeeds if cleanup changed files. If no files changed, skip this commit and record that the search was clean.

---

### Task 9: Final Build And Smoke Test

**Files:**
- No planned edits unless verification reports a bug.

- [ ] **Step 1: Build Debug x64 sample**

Run:

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild 'Dx11\TutorialApp.sln' /t:36_AdvancedAnim_Sound_Click /p:Configuration=Debug /p:Platform=x64
```

Expected: `Build succeeded`.

- [ ] **Step 2: Build Release x64 sample**

Run:

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild 'Dx11\TutorialApp.sln' /t:36_AdvancedAnim_Sound_Click /p:Configuration=Release /p:Platform=x64
```

Expected: `Build succeeded`.

- [ ] **Step 3: Launch debug executable briefly**

Run:

```powershell
$exe = Get-ChildItem -Recurse -Filter '36_AdvancedAnim_Sound_Click.exe' Dx11 | Where-Object { $_.FullName -match 'x64|Debug' } | Select-Object -First 1
Start-Process -FilePath $exe.FullName -WorkingDirectory $exe.DirectoryName -WindowStyle Hidden
Start-Sleep -Seconds 8
Get-Process 36_AdvancedAnim_Sound_Click -ErrorAction SilentlyContinue
Stop-Process -Name 36_AdvancedAnim_Sound_Click -ErrorAction SilentlyContinue
```

Expected: process appears before stop. If the executable name differs, locate it from the MSBuild output directory and rerun the same start/stop pattern.

- [ ] **Step 4: Final searches**

Run:

```powershell
rg -n "AliceDagwa|Nikke-Alice|Alice3DGame|AmazingWonderland|CaramellDansen|CaliforniaGirls|MeniShukiRushshu|RabbitHole|Specialist|Dagwa" Dx11\36_AdvancedAnim_Sound_Click Dx11\Resource
```

Expected: no active runtime references under `Dx11\36_AdvancedAnim_Sound_Click` and no old restricted files under `Dx11\Resource`.

- [ ] **Step 5: Final status**

Run:

```powershell
git status --short
```

Expected: no unstaged changes unless the user asks for a final commit that includes all task commits.

## Self-Review

- Spec coverage: asset archiving, public character staging, attribution, local animation export, external animation loading, runtime path replacement, neutral media, build verification, and old-reference search are covered.
- Placeholder scan: no deferred sections remain; every task has concrete files, commands, and expected results.
- Type consistency: the external clip type is consistently named `ExternalAnimationClipLibrary`; controller parameter and app member use the same type.
- Scope check: this stays within the advanced D3D11 sample, shared animation support, and resource tree. It does not redesign unrelated render, PMX, Live2D, or audio systems.
