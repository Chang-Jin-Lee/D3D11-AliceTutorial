# App Icon and README Branding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the third-party character icon with a rights-cleared, face-focused mascot icon across all 37 tutorial applications and add the approved two-tone explanation logo below every README title.

**Architecture:** A deterministic Pillow tool creates the checked-in icon master, multi-resolution ICO, and README banner from the two external source images. A centrally allowlisted `Directory.Build.targets` injects one shared Win32 resource into exactly the 37 solution applications, while `GameApp` loads large and small icons from each EXE. An idempotent PowerShell updater inserts one shared README logo block into the root and 37 project documents.

**Tech Stack:** Python 3 with Pillow 12.2.0, PowerShell 7/Windows PowerShell, MSBuild/Visual Studio 2022 Build Tools, Win32 resource compiler, C++20, GitHub-flavored Markdown.

## Global Constraints

- Work from `C:\Github\D3D11-AliceTutorial` on Windows 11 with Visual Studio 2022 Build Tools.
- Use bundled Python at `C:\Users\k2503200021\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe`; `python` is not on `PATH`.
- Use MSBuild at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe`.
- Apply the Win32 resource only to the 37 application projects listed in `Dx11/TutorialApp.sln`; exclude `Common` and solution-external legacy projects.
- Crop the 1254x1254 icon source at `(147, 0, 960, 960)` and output a 1024x1024 transparent master.
- Include ICO frames at exactly 16, 20, 24, 32, 40, 48, 64, 128, and 256px.
- Crop the 1536x1024 two-tone logo source at `(0, 160, 1536, 640)`.
- Do not copy the full swimsuit icon source into the repository.
- Do not modify loading screens, splash behavior, `Loading.png`, or `LoadingDone.png`.
- Do not modify `SampleModel.glb` or `SampleModel2.glb`; record and compare their SHA-256 hashes before and after execution.
- Use neutral public asset names without `ChatGPT`.
- Preserve the existing `README-NAV`, `README-INFO`, `README-RUNTIME`, and `README-NAV-BOTTOM` blocks.
- Keep `.superpowers/` scratch files untracked and out of commits.

---

## File Structure

### Create

- `tools/generate_app_brand_assets.py` — validates the two source images, crops them, removes only edge-connected white icon background, and writes final branding assets.
- `tools/tests/test_generate_app_brand_assets.py` — unit tests for crop geometry, connected-background transparency, missing-input behavior, and ICO frames.
- `Dx11/Resource/Icon/AliceTutorialIcon.png` — checked-in 1024x1024 transparent icon master.
- `Dx11/Resource/Icon/AliceTutorial.ico` — checked-in multi-resolution Windows icon.
- `docs/media/branding/alice-tutorial-logo.png` — checked-in README banner.
- `Dx11/Resource/Icon/AppIconResource.h` — shared Win32 icon resource ID.
- `Dx11/Resource/Icon/AppIcon.rc` — shared icon resource script.
- `Dx11/Directory.Build.targets` — exact 37-project allowlist and shared `ResourceCompile` injection.
- `tools/tests/test_app_icon_resource.ps1` — static contract for solution membership, MSBuild injection, resource IDs, and runtime icon loading.
- `tools/update_readme_branding.ps1` — atomic, idempotent root/project README brand block updater.
- `tools/tests/test_update_readme_branding.ps1` — fixture tests for insertion, replacement, malformed documents, and idempotence.
- `tools/tests/test_app_branding.ps1` — repository-wide branding acceptance contract.
- `tools/tests/test_built_app_icons.ps1` — verifies resource ID 101 can load at large and small sizes from freshly built EXEs.

### Modify

- `Dx11/Common/GameApp.cpp` — replace relative-file icon loading with embedded large/small resource loading and default fallback.
- `README.md` — insert one 720px `README-BRAND` block below the first H1.
- `Dx11/01_RenderingQuadangle/README.md` through `Dx11/37_Blueprint/README.md` — insert one 520px `README-BRAND` block below the first H1.

### Delete

- `Dx11/Resource/Icon/Alice.ico` — remove the third-party character icon after the new embedded path is working.

---

### Task 1: Deterministic Branding Asset Generator

**Files:**
- Create: `tools/generate_app_brand_assets.py`
- Create: `tools/tests/test_generate_app_brand_assets.py`
- Create: `Dx11/Resource/Icon/AliceTutorialIcon.png`
- Create: `Dx11/Resource/Icon/AliceTutorial.ico`
- Create: `docs/media/branding/alice-tutorial-logo.png`

**Interfaces:**
- Consumes: `Path` objects for the 1254x1254 icon source and 1536x1024 logo source.
- Produces: `remove_edge_connected_near_white(image: Image.Image, threshold: int = 248) -> Image.Image` and `generate_brand_assets(icon_source: Path, logo_source: Path, icon_master: Path, ico_output: Path, logo_output: Path) -> None`.
- Produces checked-in assets consumed by Tasks 2 and 3.

- [ ] **Step 1: Record model hashes before any implementation change**

Run:

```powershell
$paths = @(
  'Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb',
  'Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel2.glb'
)
$hashes = foreach ($path in $paths) {
  if (-not (Test-Path -LiteralPath $path)) { throw "Missing protected model: $path" }
  $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $path
  [pscustomobject]@{ Path = $path; Hash = $hash.Hash }
}
New-Item -ItemType Directory -Force -Path '.superpowers/branding' | Out-Null
$hashes | Export-Clixml -LiteralPath '.superpowers/branding/model-hashes.xml'
$hashes | Format-Table -AutoSize
```

Expected: two SHA-256 values are printed and `.superpowers/branding/model-hashes.xml` remains untracked.

- [ ] **Step 2: Write the failing Python tests**

Create `tools/tests/test_generate_app_brand_assets.py`:

```python
from pathlib import Path
import sys
import tempfile
import unittest

from PIL import Image, ImageDraw, IcoImagePlugin

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from generate_app_brand_assets import (  # noqa: E402
    ICON_SIZES,
    generate_brand_assets,
    remove_edge_connected_near_white,
)


class BrandAssetTests(unittest.TestCase):
    def test_only_edge_connected_near_white_becomes_transparent(self):
        image = Image.new("RGB", (8, 8), "white")
        draw = ImageDraw.Draw(image)
        draw.rectangle((2, 2, 5, 5), fill="black")
        draw.point((3, 3), fill="white")

        result = remove_edge_connected_near_white(image)

        self.assertEqual(result.getpixel((0, 0))[3], 0)
        self.assertEqual(result.getpixel((3, 3))[3], 255)

    def test_generation_writes_exact_dimensions_and_ico_frames(self):
        with tempfile.TemporaryDirectory() as temp:
            temp_path = Path(temp)
            icon_source = temp_path / "icon.png"
            logo_source = temp_path / "logo.png"
            icon_master = temp_path / "out" / "AliceTutorialIcon.png"
            ico_output = temp_path / "out" / "AliceTutorial.ico"
            logo_output = temp_path / "docs" / "alice-tutorial-logo.png"

            icon = Image.new("RGB", (1254, 1254), "white")
            ImageDraw.Draw(icon).rounded_rectangle((40, 10, 1214, 1214), 100, fill="#7aa9ff")
            icon.save(icon_source)
            logo = Image.new("RGB", (1536, 1024), "#202020")
            ImageDraw.Draw(logo).ellipse((620, 250, 916, 700), fill="white")
            logo.save(logo_source)

            generate_brand_assets(icon_source, logo_source, icon_master, ico_output, logo_output)

            with Image.open(icon_master) as master:
                self.assertEqual(master.size, (1024, 1024))
                self.assertEqual(master.mode, "RGBA")
                self.assertEqual(master.getpixel((0, 0))[3], 0)
            with Image.open(logo_output) as banner:
                self.assertEqual(banner.size, (1536, 640))
            with ico_output.open("rb") as stream:
                sizes = IcoImagePlugin.IcoFile(stream).sizes()
            self.assertEqual(sizes, {(size, size) for size in ICON_SIZES})

    def test_missing_source_leaves_no_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            temp_path = Path(temp)
            outputs = [temp_path / "master.png", temp_path / "icon.ico", temp_path / "logo.png"]
            with self.assertRaises(FileNotFoundError):
                generate_brand_assets(temp_path / "missing-icon.png", temp_path / "missing-logo.png", *outputs)
            self.assertTrue(all(not output.exists() for output in outputs))


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 3: Run the tests to verify they fail**

Run:

```powershell
$python = 'C:\Users\k2503200021\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $python -m unittest tools.tests.test_generate_app_brand_assets -v
```

Expected: FAIL with `ModuleNotFoundError: No module named 'generate_app_brand_assets'`.

- [ ] **Step 4: Implement the minimal generator**

Create `tools/generate_app_brand_assets.py` with these constants and functions:

```python
from __future__ import annotations

import argparse
from collections import deque
import os
from pathlib import Path

from PIL import Image

ICON_SOURCE_SIZE = (1254, 1254)
LOGO_SOURCE_SIZE = (1536, 1024)
ICON_CROP = (147, 0, 1107, 960)
LOGO_CROP = (0, 160, 1536, 800)
ICON_SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)


def remove_edge_connected_near_white(image: Image.Image, threshold: int = 248) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    width, height = rgba.size
    queue: deque[tuple[int, int]] = deque()
    visited: set[tuple[int, int]] = set()

    def is_background(x: int, y: int) -> bool:
        red, green, blue, alpha = pixels[x, y]
        return alpha > 0 and red >= threshold and green >= threshold and blue >= threshold

    for x in range(width):
        queue.append((x, 0))
        queue.append((x, height - 1))
    for y in range(height):
        queue.append((0, y))
        queue.append((width - 1, y))

    while queue:
        x, y = queue.popleft()
        if (x, y) in visited or not is_background(x, y):
            continue
        visited.add((x, y))
        red, green, blue, _ = pixels[x, y]
        pixels[x, y] = (red, green, blue, 0)
        for next_x, next_y in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= next_x < width and 0 <= next_y < height:
                queue.append((next_x, next_y))
    return rgba


def _validated_image(path: Path, expected_size: tuple[int, int]) -> Image.Image:
    if not path.is_file():
        raise FileNotFoundError(path)
    image = Image.open(path)
    image.load()
    if image.size != expected_size:
        image.close()
        raise ValueError(f"{path} must be {expected_size[0]}x{expected_size[1]}, got {image.size}")
    return image


def generate_brand_assets(
    icon_source: Path,
    logo_source: Path,
    icon_master: Path,
    ico_output: Path,
    logo_output: Path,
) -> None:
    with _validated_image(icon_source, ICON_SOURCE_SIZE) as icon_input:
        cropped_icon = icon_input.crop(ICON_CROP)
        transparent_icon = remove_edge_connected_near_white(cropped_icon)
        master = transparent_icon.resize((1024, 1024), Image.Resampling.LANCZOS)
    with _validated_image(logo_source, LOGO_SOURCE_SIZE) as logo_input:
        banner = logo_input.crop(LOGO_CROP).convert("RGB")

    outputs = (icon_master, ico_output, logo_output)
    for output in outputs:
        output.parent.mkdir(parents=True, exist_ok=True)
    temporary = {output: output.with_name(output.name + ".tmp") for output in outputs}
    try:
        master.save(temporary[icon_master], format="PNG")
        master.save(temporary[ico_output], format="ICO", sizes=[(size, size) for size in ICON_SIZES])
        banner.save(temporary[logo_output], format="PNG", optimize=True)
        for output in outputs:
            os.replace(temporary[output], output)
    finally:
        for temp_path in temporary.values():
            temp_path.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--icon-source", type=Path, required=True)
    parser.add_argument("--logo-source", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.repo_root.resolve()
    generate_brand_assets(
        args.icon_source,
        args.logo_source,
        root / "Dx11/Resource/Icon/AliceTutorialIcon.png",
        root / "Dx11/Resource/Icon/AliceTutorial.ico",
        root / "docs/media/branding/alice-tutorial-logo.png",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 5: Run the tests to verify they pass**

Run the unittest command from Step 3.

Expected: 3 tests PASS.

- [ ] **Step 6: Generate the repository assets from the supplied files**

Run:

```powershell
$python = 'C:\Users\k2503200021\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $python tools/generate_app_brand_assets.py `
  --icon-source 'C:\Users\k2503200021\Downloads\Icon_Logo\ChatGPT_Icon.png' `
  --logo-source 'C:\Users\k2503200021\Downloads\Icon_Logo\ChatGPT_TwoTone_LOGO.png' `
  --repo-root .
```

Expected: the three target assets exist; the full source images do not appear in `git status`.

- [ ] **Step 7: Inspect the generated assets and commit**

Open both PNGs with the local image viewer. Confirm the icon shows the full ears and face with no swimsuit, corner alpha is transparent, and the README banner retains the complete ears and face.

Run:

```powershell
git diff --check
git add tools/generate_app_brand_assets.py tools/tests/test_generate_app_brand_assets.py Dx11/Resource/Icon/AliceTutorialIcon.png Dx11/Resource/Icon/AliceTutorial.ico docs/media/branding/alice-tutorial-logo.png
git commit -m "feat: generate Alice Tutorial branding assets"
```

Expected: one commit containing the generator, tests, and three processed assets only.

---

### Task 2: Shared Embedded Win32 Icon Resource

**Files:**
- Create: `Dx11/Resource/Icon/AppIconResource.h`
- Create: `Dx11/Resource/Icon/AppIcon.rc`
- Create: `Dx11/Directory.Build.targets`
- Create: `tools/tests/test_app_icon_resource.ps1`
- Modify: `Dx11/Common/GameApp.cpp:1-12,127-147`
- Delete: `Dx11/Resource/Icon/Alice.ico`

**Interfaces:**
- Consumes: `AliceTutorial.ico` from Task 1 and project names from `TutorialApp.sln`.
- Produces: `IDI_ALICE_TUTORIAL_APP_ICON` with numeric resource ID 101 in each of the 37 EXEs.
- Produces: `GameApp` window classes with both `hIcon` and `hIconSm` populated.

- [ ] **Step 1: Write the failing resource contract test**

Create `tools/tests/test_app_icon_resource.ps1`:

```powershell
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$solutionText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\TutorialApp.sln')
$solutionProjects = @([regex]::Matches($solutionText, '(?m)^Project\("[^"]+"\) = "([^"]+)", "([^"]+\.vcxproj)"') |
    ForEach-Object { $_.Groups[1].Value } | Where-Object { $_ -ne 'Common' })
Assert-True ($solutionProjects.Count -eq 37) "expected 37 solution apps, got $($solutionProjects.Count)"

$targetsPath = Join-Path $repoRoot 'Dx11\Directory.Build.targets'
$headerPath = Join-Path $repoRoot 'Dx11\Resource\Icon\AppIconResource.h'
$rcPath = Join-Path $repoRoot 'Dx11\Resource\Icon\AppIcon.rc'
Assert-True (Test-Path -LiteralPath $targetsPath) 'Directory.Build.targets missing'
Assert-True (Test-Path -LiteralPath $headerPath) 'AppIconResource.h missing'
Assert-True (Test-Path -LiteralPath $rcPath) 'AppIcon.rc missing'

[xml]$targetsXml = Get-Content -Raw -LiteralPath $targetsPath
$allowlistNode = $targetsXml.SelectSingleNode("//*[local-name()='AliceTutorialBrandingProjects']")
Assert-True ($null -ne $allowlistNode) 'branding allowlist missing'
$allowlist = @($allowlistNode.InnerText.Split(';', [System.StringSplitOptions]::RemoveEmptyEntries))
Assert-True ($allowlist.Count -eq 37) "allowlist expected 37 apps, got $($allowlist.Count)"
Assert-True (@(Compare-Object ($solutionProjects | Sort-Object) ($allowlist | Sort-Object)).Count -eq 0) 'allowlist differs from TutorialApp.sln'

$targetsText = Get-Content -Raw -LiteralPath $targetsPath
$headerText = Get-Content -Raw -LiteralPath $headerPath
$rcText = Get-Content -Raw -LiteralPath $rcPath
$gameAppText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\Common\GameApp.cpp')
Assert-True ($targetsText -match 'ConfigurationType.*Application') 'Application guard missing'
Assert-True ($targetsText -match 'ResourceCompile.*AppIcon\.rc') 'shared ResourceCompile missing'
Assert-True ($headerText -match 'IDI_ALICE_TUTORIAL_APP_ICON\s+101') 'resource ID 101 missing'
Assert-True ($rcText -match 'AliceTutorial\.ico') 'ICO is not referenced by AppIcon.rc'
Assert-True ($gameAppText -match 'MAKEINTRESOURCEW\(IDI_ALICE_TUTORIAL_APP_ICON\)') 'embedded icon load missing'
Assert-True ($gameAppText -match 'SM_CXSMICON' -and $gameAppText -match 'hIconSm') 'small icon load missing'
Assert-True ($gameAppText -notmatch 'LR_LOADFROMFILE') 'relative icon file loading remains'
Assert-True ($gameAppText -notmatch 'Alice\.ico') 'old icon path remains in GameApp'
Assert-True (-not (Test-Path -LiteralPath (Join-Path $repoRoot 'Dx11\Resource\Icon\Alice.ico'))) 'old Alice.ico still exists'

'app icon resource contract tests passed'
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_app_icon_resource.ps1
```

Expected: FAIL with `Directory.Build.targets missing`.

- [ ] **Step 3: Add the shared resource ID and resource script**

Create `Dx11/Resource/Icon/AppIconResource.h`:

```cpp
#pragma once

#define IDI_ALICE_TUTORIAL_APP_ICON 101
```

Create `Dx11/Resource/Icon/AppIcon.rc`:

```rc
#include "AppIconResource.h"

IDI_ALICE_TUTORIAL_APP_ICON ICON "AliceTutorial.ico"
```

- [ ] **Step 4: Add the exact centralized project allowlist**

Create `Dx11/Directory.Build.targets`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <AliceTutorialBrandingProjects>;01_RenderingQuadangle;02_RenderingCube;03_RenderingMeshAndSceneGraph;04_RenderingMeshWithTexture;05_Mesh;06_pmx;07_pmxTexture;08_ImguiSystemInfo;09_Lighting;10_StaticCube_SkyBox;11_Live2D;12_Lighting_BlinnPhong;13_LineRenderer_AxisDebug;14_Lighting_Phong;15_pmxWithPhong;16_NormalMapping;17_fbx_pmx_obj_WithPhong;18_fbx_Animation;19_MultiModels;20_Depth_And_Alpha_Issue;21_MultiModels_With_Animations;22_VMD;23_Rigid_Animation;24_Skinned_With_Bone_Structure;25_ToonShading_Outline;26_ShadowMap_PCF;27_DebugDraw;28_Scene_Shared3DModel_Animation;29_MousePicking;30_PBR_BRDF;31_IBL;32_Sound_FMOD;33_Sound_Animation_Camera_Motion;34_ToneMapping;35_DeferredRendering;36_AdvancedAnim_Sound_Click;37_Blueprint;</AliceTutorialBrandingProjects>
    <AliceTutorialBrandingEnabled Condition="'$(ConfigurationType)' == 'Application'">$([System.String]::Copy('$(AliceTutorialBrandingProjects)').Contains(';$(MSBuildProjectName);'))</AliceTutorialBrandingEnabled>
  </PropertyGroup>
  <ItemGroup Condition="'$(AliceTutorialBrandingEnabled)' == 'True'">
    <ResourceCompile Include="$(MSBuildThisFileDirectory)Resource\Icon\AppIcon.rc" />
  </ItemGroup>
  <ItemDefinitionGroup Condition="'$(AliceTutorialBrandingEnabled)' == 'True'">
    <ResourceCompile>
      <AdditionalIncludeDirectories>$(MSBuildThisFileDirectory)Resource\Icon;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ResourceCompile>
  </ItemDefinitionGroup>
</Project>
```

- [ ] **Step 5: Replace relative icon loading in `GameApp.cpp`**

Add this include after the existing local includes:

```cpp
#include "../Resource/Icon/AppIconResource.h"
```

Add this helper in the existing unnamed namespace, or create one immediately below the includes:

```cpp
namespace
{
    HICON LoadEmbeddedApplicationIcon(HINSTANCE instance, int width, int height)
    {
        HICON icon = reinterpret_cast<HICON>(LoadImageW(
            instance,
            MAKEINTRESOURCEW(IDI_ALICE_TUTORIAL_APP_ICON),
            IMAGE_ICON,
            width,
            height,
            LR_DEFAULTCOLOR | LR_SHARED));
        return icon ? icon : LoadIconW(nullptr, IDI_APPLICATION);
    }
}
```

Replace the `LR_LOADFROMFILE` block in `GameApp::Run` with:

```cpp
m_wcex.hIcon = LoadEmbeddedApplicationIcon(
    hInstance,
    GetSystemMetrics(SM_CXICON),
    GetSystemMetrics(SM_CYICON));
m_wcex.hIconSm = LoadEmbeddedApplicationIcon(
    hInstance,
    GetSystemMetrics(SM_CXSMICON),
    GetSystemMetrics(SM_CYSMICON));
```

Delete `Dx11/Resource/Icon/Alice.ico` only after the new code and resources exist.

- [ ] **Step 6: Run the contract test and representative builds**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_app_icon_resource.ps1
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
& $msbuild 'Dx11\TutorialApp.sln' /m "/t:01_RenderingQuadangle;36_AdvancedAnim_Sound_Click;37_Blueprint" /p:Configuration=Debug /p:Platform=x64 /verbosity:minimal
```

Expected: contract test PASS and the solution target build exits 0 for all three applications and their dependencies. If an external SDK blocks one representative build, capture the exact missing file or library and continue only after confirming the icon/resource compilation itself passed.

- [ ] **Step 7: Commit the shared icon resource change**

Run:

```powershell
git add Dx11/Directory.Build.targets Dx11/Resource/Icon/AppIconResource.h Dx11/Resource/Icon/AppIcon.rc Dx11/Common/GameApp.cpp tools/tests/test_app_icon_resource.ps1
git add -u -- Dx11/Resource/Icon/Alice.ico
git commit -m "feat: embed shared app icon in tutorial executables"
```

Expected: one commit with the target, resource files, runtime loader, test, and old-icon deletion.

---

### Task 3: Idempotent README Branding Updater

**Files:**
- Create: `tools/update_readme_branding.ps1`
- Create: `tools/tests/test_update_readme_branding.ps1`
- Modify: `README.md`
- Modify: the 37 project README files listed by `tools/readme_media_manifest.json`

**Interfaces:**
- Consumes: `tools/readme_media_manifest.json` and `docs/media/branding/alice-tutorial-logo.png`.
- Produces: `README-BRAND:START/END` block immediately after the first H1.
- Command: `tools/update_readme_branding.ps1 [-RepoRoot <path>] [-Manifest <path>]`.

- [ ] **Step 1: Write the failing updater tests**

Create `tools/tests/test_update_readme_branding.ps1` with a temporary repository containing a root README, two selected project READMEs, a two-project manifest, and an unselected README. Assert all of the following in executable PowerShell assertions:

```powershell
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$updater = Join-Path $repoRoot 'tools\update_readme_branding.ps1'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('readme-brand-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Force -Path (Join-Path $fixture 'tools'), (Join-Path $fixture 'Dx11\01_Test'), (Join-Path $fixture 'Dx11\02_Test'), (Join-Path $fixture 'Dx11\03_Unselected') | Out-Null
    @{ expectedProjectCount = 2; projects = @(@{ directory = '01_Test' }, @{ directory = '02_Test' }) } |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $fixture 'tools\manifest.json') -Encoding utf8NoBOM
    [IO.File]::WriteAllText((Join-Path $fixture 'README.md'), "# Root`r`n`r`nRoot body`r`n", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\01_Test\README.md'), "<!-- README-NAV-TOP:START -->`nnav`n<!-- README-NAV-TOP:END -->`n`n# One`n`nBody one`n", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\02_Test\README.md'), "# Two`n`nBody two`n", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\03_Unselected\README.md'), "# Three`n", [Text.UTF8Encoding]::new($false))
    $unselectedBefore = [IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\03_Unselected\README.md'))

    & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json'
    $rootFirst = Get-Content -Raw -LiteralPath (Join-Path $fixture 'README.md')
    $oneFirst = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\01_Test\README.md')
    & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json'
    $oneSecond = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\01_Test\README.md')

    Assert-True ($rootFirst -match 'width="720"') 'root width must be 720'
    Assert-True ($oneFirst -match 'width="520"') 'project width must be 520'
    Assert-True (($oneFirst | Select-String 'README-BRAND:START' -AllMatches).Matches.Count -eq 1) 'brand block duplicated'
    Assert-True ($oneFirst -ceq $oneSecond) 'second run must be idempotent'
    Assert-True ($oneFirst.IndexOf('README-BRAND:START') -gt $oneFirst.IndexOf('# One')) 'brand block must follow H1'
    Assert-True ($oneFirst -match 'README-NAV-TOP:START') 'existing navigation marker changed'
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($unselectedBefore, [IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\03_Unselected\README.md')))) 'unselected README changed'

    $beforeMissingH1 = [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md'))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\02_Test\README.md'), "No heading`n", [Text.UTF8Encoding]::new($false))
    $failed = $false
    try { & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json' } catch { $failed = $_.Exception.Message -match 'first H1' }
    Assert-True $failed 'missing H1 must fail'
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($beforeMissingH1, [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md')))) 'validation failure partially updated root README'

    'README branding updater tests passed'
}
finally {
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_update_readme_branding.ps1
```

Expected: FAIL because `tools/update_readme_branding.ps1` does not exist.

- [ ] **Step 3: Implement the updater with validate-then-write behavior**

Create `tools/update_readme_branding.ps1` with these exact rules:

```powershell
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Manifest = 'tools/readme_media_manifest.json'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = [IO.Path]::GetFullPath($RepoRoot)
$manifestPath = if ([IO.Path]::IsPathRooted($Manifest)) { $Manifest } else { Join-Path $root $Manifest }
$data = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
if (@($data.projects).Count -ne [int]$data.expectedProjectCount) { throw 'manifest project count mismatch' }

function New-BrandBlock([string]$ImagePath, [int]$Width, [string]$Newline) {
    return @(
        '<!-- README-BRAND:START -->',
        "<p align=`"center`"><img src=`"$ImagePath`" width=`"$Width`" alt=`"D3D11 Alice Tutorial mascot logo`" /></p>",
        '<!-- README-BRAND:END -->'
    ) -join $Newline
}

function Get-UpdatedReadme([string]$Path, [string]$ImagePath, [int]$Width) {
    $content = [IO.File]::ReadAllText($Path, [Text.UTF8Encoding]::new($false, $true))
    $newline = if ($content.Contains("`r`n")) { "`r`n" } else { "`n" }
    $start = '<!-- README-BRAND:START -->'
    $end = '<!-- README-BRAND:END -->'
    $startCount = ([regex]::Matches($content, [regex]::Escape($start))).Count
    $endCount = ([regex]::Matches($content, [regex]::Escape($end))).Count
    if ($startCount -ne $endCount -or $startCount -gt 1) { throw "malformed README-BRAND markers: $Path" }
    $block = New-BrandBlock $ImagePath $Width $newline
    if ($startCount -eq 1) {
        $pattern = [regex]::Escape($start) + '.*?' + [regex]::Escape($end)
        return [regex]::Replace($content, $pattern, [Text.RegularExpressions.MatchEvaluator]{ param($match) $block }, [Text.RegularExpressions.RegexOptions]::Singleline)
    }
    $heading = [regex]::Match($content, '(?m)^# .+$')
    if (-not $heading.Success) { throw "first H1 missing: $Path" }
    $insertAt = $heading.Index + $heading.Length
    return $content.Insert($insertAt, $newline + $newline + $block)
}

$targets = [Collections.Generic.List[object]]::new()
$targets.Add([pscustomobject]@{ Path = Join-Path $root 'README.md'; Image = 'docs/media/branding/alice-tutorial-logo.png'; Width = 720 })
foreach ($project in @($data.projects)) {
    if ([string]$project.directory -notmatch '^\d{2}_[A-Za-z0-9_]+$') { throw "unsafe project directory: $($project.directory)" }
    $targets.Add([pscustomobject]@{ Path = Join-Path $root "Dx11/$($project.directory)/README.md"; Image = '../../docs/media/branding/alice-tutorial-logo.png'; Width = 520 })
}

$updates = foreach ($target in $targets) {
    if (-not (Test-Path -LiteralPath $target.Path)) { throw "README missing: $($target.Path)" }
    [pscustomobject]@{ Path = $target.Path; Content = Get-UpdatedReadme $target.Path $target.Image $target.Width }
}

$encoding = [Text.UTF8Encoding]::new($false)
$temporary = @()
try {
    foreach ($update in $updates) {
        $temp = $update.Path + '.brand.tmp'
        [IO.File]::WriteAllText($temp, $update.Content, $encoding)
        $temporary += [pscustomobject]@{ Temp = $temp; Destination = $update.Path }
    }
    foreach ($file in $temporary) { Move-Item -LiteralPath $file.Temp -Destination $file.Destination -Force }
}
finally {
    foreach ($file in $temporary) { if (Test-Path -LiteralPath $file.Temp) { Remove-Item -LiteralPath $file.Temp -Force } }
}
```

- [ ] **Step 4: Run the updater tests to verify they pass**

Run the command from Step 2.

Expected: `README branding updater tests passed`.

- [ ] **Step 5: Apply the updater twice to the real repository**

Run:

```powershell
pwsh -NoProfile -File tools/update_readme_branding.ps1
$firstDiff = git diff -- README.md 'Dx11/*/README.md'
pwsh -NoProfile -File tools/update_readme_branding.ps1
$secondDiff = git diff -- README.md 'Dx11/*/README.md'
if ($firstDiff -cne $secondDiff) { throw 'README branding update is not idempotent' }
```

Expected: root plus exactly 37 project READMEs change, and the second run creates no additional diff.

- [ ] **Step 6: Run existing README regression tests and commit**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project_readme_updater.ps1
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
git diff --check
git add tools/update_readme_branding.ps1 tools/tests/test_update_readme_branding.ps1 README.md Dx11/*/README.md
git commit -m "docs: add Alice Tutorial branding to READMEs"
```

Expected: all tests PASS and the commit contains the updater, its test, root README, and 37 project READMEs.

---

### Task 4: Repository and Built-EXE Acceptance Verification

**Files:**
- Create: `tools/tests/test_app_branding.ps1`
- Create: `tools/tests/test_built_app_icons.ps1`

**Interfaces:**
- Consumes: all outputs from Tasks 1-3.
- Produces: one source-tree acceptance command and one built-binary icon resource command.

- [ ] **Step 1: Write the repository-wide acceptance test**

Create `tools/tests/test_app_branding.ps1` to load `tools/readme_media_manifest.json`, assert `expectedProjectCount == 37`, and perform these exact checks:

```powershell
$ErrorActionPreference = 'Stop'
function Assert-True([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\readme_media_manifest.json') | ConvertFrom-Json
Assert-True ([int]$manifest.expectedProjectCount -eq 37) 'expectedProjectCount must be 37'
Assert-True (@($manifest.projects).Count -eq 37) 'manifest must contain 37 projects'

$logo = Join-Path $repoRoot 'docs\media\branding\alice-tutorial-logo.png'
Assert-True (Test-Path -LiteralPath $logo) 'shared README logo missing'
$readmes = @([pscustomobject]@{ Path = Join-Path $repoRoot 'README.md'; Relative = 'docs/media/branding/alice-tutorial-logo.png'; Width = 720 })
$readmes += @($manifest.projects | ForEach-Object { [pscustomobject]@{ Path = Join-Path $repoRoot "Dx11/$($_.directory)/README.md"; Relative = '../../docs/media/branding/alice-tutorial-logo.png'; Width = 520 } })
Assert-True ($readmes.Count -eq 38) 'expected root plus 37 project READMEs'
foreach ($entry in $readmes) {
    $content = Get-Content -Raw -LiteralPath $entry.Path
    Assert-True (([regex]::Matches($content, '<!-- README-BRAND:START -->')).Count -eq 1) "brand start count invalid: $($entry.Path)"
    Assert-True (([regex]::Matches($content, '<!-- README-BRAND:END -->')).Count -eq 1) "brand end count invalid: $($entry.Path)"
    Assert-True ($content -match [regex]::Escape("src=`"$($entry.Relative)`"")) "logo path invalid: $($entry.Path)"
    Assert-True ($content -match "width=`"$($entry.Width)`"") "logo width invalid: $($entry.Path)"
    $h1 = [regex]::Match($content, '(?m)^# .+$')
    $brand = $content.IndexOf('<!-- README-BRAND:START -->')
    Assert-True ($h1.Success -and $brand -gt ($h1.Index + $h1.Length)) "brand block must follow first H1: $($entry.Path)"
}

$allText = (Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Dx11') -Recurse -File -Include *.cpp,*.h,*.rc,*.targets | Get-Content -Raw) -join "`n"
Assert-True ($allText -notmatch 'Resource\\Icon\\Alice\.ico') 'old icon reference remains'
Assert-True (-not (Test-Path -LiteralPath (Join-Path $repoRoot 'Dx11\Resource\Icon\Alice.ico'))) 'old icon file remains'
Assert-True ($allText -notmatch 'ChatGPT_Icon|ChatGPT_TwoTone_LOGO') 'source filenames leaked into public files'

'app branding acceptance tests passed'
```

- [ ] **Step 2: Write the built-EXE icon resource verifier**

Create `tools/tests/test_built_app_icons.ps1`:

```powershell
param(
    [Parameter(Mandatory = $true)][string]$BinRoot,
    [Parameter(Mandatory = $true)][datetime]$NotOlderThan,
    [string[]]$ProjectNames = @()
)
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeIconProbe {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool FreeLibrary(IntPtr module);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadImageW(IntPtr instance, IntPtr name, uint type, int width, int height, uint flags);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool DestroyIcon(IntPtr icon);
}
'@

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$resolvedBinRoot = if ([IO.Path]::IsPathRooted($BinRoot)) { (Resolve-Path $BinRoot).Path } else { (Resolve-Path (Join-Path $repoRoot $BinRoot)).Path }
if ($ProjectNames.Count -eq 0) {
    $solutionText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\TutorialApp.sln')
    $ProjectNames = @([regex]::Matches($solutionText, '(?m)^Project\("[^"]+"\) = "([^"]+)", "([^"]+\.vcxproj)"') |
        ForEach-Object { $_.Groups[1].Value } | Where-Object { $_ -ne 'Common' })
}
Assert-True ($ProjectNames.Count -gt 0) 'no application projects selected'

foreach ($projectName in $ProjectNames) {
    $matches = @(Get-ChildItem -LiteralPath $resolvedBinRoot -Recurse -File -Filter "$projectName.exe" |
        Where-Object { $_.LastWriteTime -ge $NotOlderThan })
    Assert-True ($matches.Count -eq 1) "expected one fresh $projectName.exe, got $($matches.Count)"
    $module = [NativeIconProbe]::LoadLibraryExW($matches[0].FullName, [IntPtr]::Zero, 0x00000002)
    Assert-True ($module -ne [IntPtr]::Zero) "LoadLibraryExW failed for $projectName with $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    try {
        foreach ($size in @(32, 16)) {
            $icon = [NativeIconProbe]::LoadImageW($module, [IntPtr]::new(101), 1, $size, $size, 0)
            Assert-True ($icon -ne [IntPtr]::Zero) "resource 101 ${size}px missing from $projectName"
            $null = [NativeIconProbe]::DestroyIcon($icon)
        }
    }
    finally {
        $null = [NativeIconProbe]::FreeLibrary($module)
    }
}

"built app icon resources verified: $($ProjectNames.Count) executables"
```

- [ ] **Step 3: Run the source-tree acceptance test**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_app_branding.ps1
```

Expected: `app branding acceptance tests passed`.

- [ ] **Step 4: Build the complete x64 Debug solution**

Run:

```powershell
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
$buildStarted = Get-Date
& $msbuild 'Dx11\TutorialApp.sln' /m /t:Build /p:Configuration=Debug /p:Platform=x64 /verbosity:minimal
if ($LASTEXITCODE -ne 0) { throw "full solution build failed with exit code $LASTEXITCODE" }
pwsh -NoProfile -File tools/tests/test_built_app_icons.ps1 -BinRoot 'Dx11' -NotOlderThan $buildStarted
```

Expected: solution build exits 0 and all 37 fresh EXEs expose resource 101 at 32x32 and 16x16.

If the full build is blocked by a pre-existing external SDK or library, save the complete MSBuild error, then run the representative fallback without claiming a full-solution pass:

```powershell
$buildStarted = Get-Date
& $msbuild 'Dx11\TutorialApp.sln' /m "/t:Common;01_RenderingQuadangle;36_AdvancedAnim_Sound_Click;37_Blueprint" /p:Configuration=Debug /p:Platform=x64 /verbosity:minimal
if ($LASTEXITCODE -ne 0) { throw "representative build failed with exit code $LASTEXITCODE" }
pwsh -NoProfile -File tools/tests/test_built_app_icons.ps1 -BinRoot 'Dx11' -NotOlderThan $buildStarted -ProjectNames @('01_RenderingQuadangle', '36_AdvancedAnim_Sound_Click', '37_Blueprint')
```

- [ ] **Step 5: Run the complete regression set and verify protected model hashes**

Run:

```powershell
$python = 'C:\Users\k2503200021\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $python -m unittest tools.tests.test_generate_app_brand_assets -v
pwsh -NoProfile -File tools/tests/test_app_icon_resource.ps1
pwsh -NoProfile -File tools/tests/test_update_readme_branding.ps1
pwsh -NoProfile -File tools/tests/test_app_branding.ps1
pwsh -NoProfile -File tools/tests/test_project_readme_updater.ps1
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
git diff --check

$before = Import-Clixml -LiteralPath '.superpowers/branding/model-hashes.xml'
foreach ($record in $before) {
    $after = (Get-FileHash -Algorithm SHA256 -LiteralPath $record.Path).Hash
    if ($after -ne $record.Hash) { throw "protected model changed: $($record.Path)" }
}
```

Expected: all tests PASS, `git diff --check` is silent, and both protected model hashes match.

- [ ] **Step 6: Commit the acceptance verifiers**

Run:

```powershell
git add tools/tests/test_app_branding.ps1 tools/tests/test_built_app_icons.ps1
git commit -m "test: verify app branding integration"
```

Expected: one test-only commit. Report whether verification covered all 37 builds or the documented representative fallback.

---

## Final Review Checklist

- The full source files from Downloads are not tracked.
- `AliceTutorialIcon.png` has transparent outer corners and no swimsuit pixels.
- `AliceTutorial.ico` contains all nine required frames.
- `Alice.ico` and `LR_LOADFROMFILE` icon loading are absent.
- The exact 37-project target allowlist matches `TutorialApp.sln`.
- `GameApp` sets both `hIcon` and `hIconSm` with default fallbacks.
- Root README has one 720px brand block after its first H1.
- Each of the 37 project READMEs has one 520px brand block after its first H1.
- No loading or splash asset changed.
- `SampleModel.glb` and `SampleModel2.glb` hashes match the pre-work snapshot.
- User-authored model changes and `.superpowers/` scratch data are absent from every branding commit.
