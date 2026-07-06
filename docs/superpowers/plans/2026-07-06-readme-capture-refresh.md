# README Capture Refresh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the root README's old external preview media with repo-local screenshots for projects 01 through 37 and one representative GIF for project 36, while preserving the current README as `README_old.md`.

**Architecture:** Add a small repo-local capture workflow driven by a JSON manifest, use it to launch each built sample from `Dx11/bin`, store captures under `docs/media/readme/`, then update the root README to reference those media files. Startup camera/object transforms are adjusted only in sample app defaults when the first-frame capture is not readable.

**Tech Stack:** Visual Studio/MSBuild for `Debug|x64`, PowerShell 7/Windows PowerShell with Win32 interop for screenshots, `C:\ffmpeg\bin\ffmpeg.exe` for GIF encoding, repository-local Markdown media links.

---

## File Structure

**Create:**
- `tools/readme_media_manifest.json` - authoritative list of README sample executables, output images, links, and capture delays.
- `tools/capture_readme_media.ps1` - launches samples, captures screenshots, records GIF frames for project 36, and writes `docs/media/readme/capture-report.md`.
- `tools/verify_readme_media.ps1` - checks README media links, file existence, nonzero sizes, and forbidden external attachment URLs.
- `docs/media/readme/` - generated screenshots, GIF, and capture report.
- `README_old.md` - exact legacy copy of the current root `README.md`.

**Modify:**
- `README.md` - keep existing Korean structure, replace preview images with repo-local `docs/media/readme/` paths, add the project 36 GIF near the representative demo section.
- Camera/object startup defaults only if capture audit shows unreadable first frames. Primary candidate files are:
  - `Dx11/01_RenderingQuadangle/App.cpp`
  - `Dx11/02_RenderingCube/App.cpp`
  - `Dx11/02_RenderingCube/App.h`
  - `Dx11/03_RenderingMeshAndSceneGraph/App.cpp`
  - `Dx11/03_RenderingMeshAndSceneGraph/App.h`
  - `Dx11/04_RenderingMeshWithTexture/App.cpp`
  - `Dx11/04_RenderingMeshWithTexture/App.h`
  - `Dx11/05_Mesh/App.cpp`
  - `Dx11/05_Mesh/App.h`
  - `Dx11/06_pmx/App.cpp`
  - `Dx11/06_pmx/App.h`
  - `Dx11/07_pmxTexture/App.cpp`
  - `Dx11/07_pmxTexture/App.h`
  - `Dx11/08_ImguiSystemInfo/App.cpp`
  - `Dx11/08_ImguiSystemInfo/App.h`
  - `Dx11/09_Lighting/App.cpp`
  - `Dx11/09_Lighting/App.h`
  - `Dx11/10_StaticCube_SkyBox/App.cpp`
  - `Dx11/10_StaticCube_SkyBox/App.h`
  - `Dx11/11_Live2D/App.cpp`
  - `Dx11/11_Live2D/App.h`
  - `Dx11/12_Lighting_BlinnPhong/App.cpp`
  - `Dx11/12_Lighting_BlinnPhong/App.h`
  - `Dx11/13_LineRenderer_AxisDebug/App.cpp`
  - `Dx11/13_LineRenderer_AxisDebug/App.h`
  - `Dx11/14_Lighting_Phong/App.cpp`
  - `Dx11/14_Lighting_Phong/App.h`
  - `Dx11/15_pmxWithPhong/App.cpp`
  - `Dx11/15_pmxWithPhong/App.h`
  - `Dx11/16_NormalMapping/App.cpp`
  - `Dx11/16_NormalMapping/App.h`
  - `Dx11/17_fbx_pmx_obj_WithPhong/App.cpp`
  - `Dx11/17_fbx_pmx_obj_WithPhong/App.h`
  - `Dx11/18_fbx_Animation/App.cpp`
  - `Dx11/18_fbx_Animation/App.h`
  - `Dx11/19_MultiModels/App.cpp`
  - `Dx11/19_MultiModels/App.h`
  - `Dx11/20_Depth_And_Alpha_Issue/App.cpp`
  - `Dx11/20_Depth_And_Alpha_Issue/App.h`
  - `Dx11/21_MultiModels_With_Animations/App.cpp`
  - `Dx11/21_MultiModels_With_Animations/App.h`
  - `Dx11/22_VMD/App.cpp`
  - `Dx11/22_VMD/App.h`
  - `Dx11/23_Rigid_Animation/App.cpp`
  - `Dx11/23_Rigid_Animation/App.h`
  - `Dx11/24_Skinned_With_Bone_Structure/App.cpp`
  - `Dx11/24_Skinned_With_Bone_Structure/App.h`
  - `Dx11/25_ToonShading_Outline/App.cpp`
  - `Dx11/25_ToonShading_Outline/App.h`
  - `Dx11/26_ShadowMap_PCF/App.cpp`
  - `Dx11/26_ShadowMap_PCF/App.h`
  - `Dx11/27_DebugDraw/App.cpp`
  - `Dx11/27_DebugDraw/App.h`
  - `Dx11/28_Scene_Shared3DModel_Animation/App.cpp`
  - `Dx11/28_Scene_Shared3DModel_Animation/App.h`
  - `Dx11/29_MousePicking/App.cpp`
  - `Dx11/29_MousePicking/App.h`
  - `Dx11/30_PBR_BRDF/App.cpp`
  - `Dx11/30_PBR_BRDF/App.h`
  - `Dx11/31_IBL/App.cpp`
  - `Dx11/31_IBL/App.h`
  - `Dx11/32_Sound_FMOD/App.cpp`
  - `Dx11/32_Sound_FMOD/App.h`
  - `Dx11/33_Sound_Animation_Camera_Motion/App.cpp`
  - `Dx11/33_Sound_Animation_Camera_Motion/App.h`
  - `Dx11/34_ToneMapping/App.cpp`
  - `Dx11/34_ToneMapping/App.h`
  - `Dx11/35_DeferredRendering/App.cpp`
  - `Dx11/35_DeferredRendering/App.h`
  - `Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl`
  - `Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl`
  - `Dx11/37_Blueprint/App.cpp`
  - `Dx11/37_Blueprint/App.h`

---

### Task 1: Add README Media Manifest

**Files:**
- Create: `tools/readme_media_manifest.json`
- Create: `docs/media/readme/.gitkeep`

- [ ] **Step 1: Create the media directory**

Run:

```powershell
New-Item -ItemType Directory -Force docs\media\readme | Out-Null
New-Item -ItemType File -Force docs\media\readme\.gitkeep | Out-Null
```

Expected: `docs\media\readme` exists.

- [ ] **Step 2: Create `tools/readme_media_manifest.json`**

Use this exact JSON content:

```json
{
  "runtimeDir": "Dx11/bin",
  "mediaDir": "docs/media/readme",
  "defaultDelayMs": 2500,
  "defaultWidth": 1280,
  "defaultHeight": 720,
  "projects": [
    {"number": "01", "name": "RenderingQuadangle", "directory": "01_RenderingQuadangle", "exe": "01_RenderingQuadangle.exe", "image": "01-RenderingQuadangle.png", "delayMs": 1500},
    {"number": "02", "name": "RenderingCube", "directory": "02_RenderingCube", "exe": "02_RenderingCube.exe", "image": "02-RenderingCube.png", "delayMs": 1500},
    {"number": "03", "name": "RenderingMeshAndSceneGraph", "directory": "03_RenderingMeshAndSceneGraph", "exe": "03_RenderingMeshAndSceneGraph.exe", "image": "03-RenderingMeshAndSceneGraph.png", "delayMs": 1500},
    {"number": "04", "name": "RenderingMeshWithTexture", "directory": "04_RenderingMeshWithTexture", "exe": "04_RenderingMeshWithTexture.exe", "image": "04-RenderingMeshWithTexture.png", "delayMs": 1500},
    {"number": "05", "name": "MeshFBX", "directory": "05_Mesh", "exe": "05_Mesh.exe", "image": "05-Mesh.png", "delayMs": 2000},
    {"number": "06", "name": "PMX A-Pose", "directory": "06_pmx", "exe": "06_pmx.exe", "image": "06-pmx.png", "delayMs": 2500},
    {"number": "07", "name": "PMX Texture", "directory": "07_pmxTexture", "exe": "07_pmxTexture.exe", "image": "07-pmxTexture.png", "delayMs": 2500},
    {"number": "08", "name": "ImguiSystemInfo", "directory": "08_ImguiSystemInfo", "exe": "08_ImguiSystemInfo.exe", "image": "08-ImguiSystemInfo.png", "delayMs": 1500},
    {"number": "09", "name": "Lighting", "directory": "09_Lighting", "exe": "09_Lighting.exe", "image": "09-Lighting.png", "delayMs": 1500},
    {"number": "10", "name": "Static Cube SkyBox", "directory": "10_StaticCube_SkyBox", "exe": "10_StaticCube_SkyBox.exe", "image": "10-StaticCube-SkyBox.png", "delayMs": 2000},
    {"number": "11", "name": "Live2D", "directory": "11_Live2D", "exe": "11_Live2D.exe", "image": "11-Live2D.png", "delayMs": 2500},
    {"number": "12", "name": "Lighting Blinn Phong", "directory": "12_Lighting_BlinnPhong", "exe": "12_Lighting_BlinnPhong.exe", "image": "12-Lighting-BlinnPhong.png", "delayMs": 1500},
    {"number": "13", "name": "LineRenderer AxisDebug", "directory": "13_LineRenderer_AxisDebug", "exe": "13_LineRenderer_AxisDebug.exe", "image": "13-LineRenderer-AxisDebug.png", "delayMs": 1500},
    {"number": "14", "name": "Lighting Phong", "directory": "14_Lighting_Phong", "exe": "14_Lighting_Phong.exe", "image": "14-Lighting-Phong.png", "delayMs": 1500},
    {"number": "15", "name": "pmx With Phong", "directory": "15_pmxWithPhong", "exe": "15_pmxWithPhong.exe", "image": "15-pmxWithPhong.png", "delayMs": 2500},
    {"number": "16", "name": "Texture Normal Mapping", "directory": "16_NormalMapping", "exe": "16_NormalMapping.exe", "image": "16-NormalMapping.png", "delayMs": 1500},
    {"number": "17", "name": "Render fbx pmx obj", "directory": "17_fbx_pmx_obj_WithPhong", "exe": "17_fbx_pmx_obj_WithPhong.exe", "image": "17-fbx-pmx-obj-WithPhong.png", "delayMs": 2500},
    {"number": "18", "name": "fbx Animation", "directory": "18_fbx_Animation", "exe": "18_fbx_Animation.exe", "image": "18-fbx-Animation.png", "delayMs": 2500},
    {"number": "19", "name": "MultiModels", "directory": "19_MultiModels", "exe": "19_MultiModels.exe", "image": "19-MultiModels.png", "delayMs": 2500},
    {"number": "20", "name": "Depth And Alpha Issue", "directory": "20_Depth_And_Alpha_Issue", "exe": "20_Depth_And_Alpha_Issue.exe", "image": "20-Depth-And-Alpha-Issue.png", "delayMs": 2000},
    {"number": "21", "name": "MultiModels With Animations", "directory": "21_MultiModels_With_Animations", "exe": "21_MultiModels_With_Animations.exe", "image": "21-MultiModels-With-Animations.png", "delayMs": 3000},
    {"number": "22", "name": "VMD Camera", "directory": "22_VMD", "exe": "22_VMD.exe", "image": "22-VMD.png", "delayMs": 2500},
    {"number": "23", "name": "Rigid, Skinned Animation", "directory": "23_Rigid_Animation", "exe": "23_Rigid_Animation.exe", "image": "23-Rigid-Animation.png", "delayMs": 2500},
    {"number": "24", "name": "Skinned With Bone Structure", "directory": "24_Skinned_With_Bone_Structure", "exe": "24_Skinned_With_Bone_Structure.exe", "image": "24-Skinned-With-Bone-Structure.png", "delayMs": 2500},
    {"number": "25", "name": "ToonShading Outline", "directory": "25_ToonShading_Outline", "exe": "25_ToonShading_Outline.exe", "image": "25-ToonShading-Outline.png", "delayMs": 2500},
    {"number": "26", "name": "ShadowMap PCF", "directory": "26_ShadowMap_PCF", "exe": "26_ShadowMap_PCF.exe", "image": "26-ShadowMap-PCF.png", "delayMs": 2500},
    {"number": "27", "name": "debug draw box", "directory": "27_DebugDraw", "exe": "27_DebugDraw.exe", "image": "27-DebugDraw.png", "delayMs": 2500},
    {"number": "28", "name": "Scene Shared3DModel Animation", "directory": "28_Scene_Shared3DModel_Animation", "exe": "28_Scene_Shared3DModel_Animation.exe", "image": "28-Scene-Shared3DModel-Animation.png", "delayMs": 3000},
    {"number": "29", "name": "Mouse Picking", "directory": "29_MousePicking", "exe": "29_MousePicking.exe", "image": "29-MousePicking.png", "delayMs": 2500},
    {"number": "30", "name": "PBR BRDF", "directory": "30_PBR_BRDF", "exe": "30_PBR_BRDF.exe", "image": "30-PBR-BRDF.png", "delayMs": 2500},
    {"number": "31", "name": "IBL Image Based Lighting", "directory": "31_IBL", "exe": "31_IBL.exe", "image": "31-IBL.png", "delayMs": 2500},
    {"number": "32", "name": "Sound FMOD", "directory": "32_Sound_FMOD", "exe": "32_Sound_FMOD.exe", "image": "32-Sound-FMOD.png", "delayMs": 2500},
    {"number": "33", "name": "Sound Animation Camera Motion", "directory": "33_Sound_Animation_Camera_Motion", "exe": "33_Sound_Animation_Camera_Motion.exe", "image": "33-Sound-Animation-Camera-Motion.png", "delayMs": 3000},
    {"number": "34", "name": "Tone Mapping", "directory": "34_ToneMapping", "exe": "34_ToneMapping.exe", "image": "34-ToneMapping.png", "delayMs": 2500},
    {"number": "35", "name": "Deferred Rendering", "directory": "35_DeferredRendering", "exe": "35_DeferredRendering.exe", "image": "35-DeferredRendering.png", "delayMs": 2500},
    {"number": "36", "name": "Animation+", "directory": "36_AdvancedAnim_Sound_Click", "exe": "36_AdvancedAnim_Sound_Click.exe", "image": "36-AdvancedAnim-Sound-Click.png", "gif": "36-advanced-anim-sound-click.gif", "delayMs": 4500, "gifSeconds": 4, "gifFps": 10},
    {"number": "37", "name": "imgui-node-editor demo", "directory": "37_Blueprint", "exe": "37_Blueprint.exe", "image": "37-Blueprint.png", "delayMs": 2000}
  ]
}
```

- [ ] **Step 3: Verify the manifest parses**

Run:

```powershell
$json = Get-Content -Raw tools\readme_media_manifest.json | ConvertFrom-Json
if ($json.projects.Count -ne 37) { throw "Expected 37 projects, got $($json.projects.Count)" }
$json.projects | ForEach-Object {
  if (-not $_.exe -or -not $_.image -or -not $_.directory) { throw "Manifest entry missing fields: $($_ | ConvertTo-Json -Compress)" }
}
"Manifest OK: $($json.projects.Count) projects"
```

Expected: `Manifest OK: 37 projects`.

- [ ] **Step 4: Commit manifest**

Run:

```powershell
git add tools/readme_media_manifest.json docs/media/readme/.gitkeep
git commit -m "chore: add readme media manifest"
```

Expected: commit succeeds.

---

### Task 2: Add Capture And Verification Tooling

**Files:**
- Create: `tools/capture_readme_media.ps1`
- Create: `tools/verify_readme_media.ps1`

- [ ] **Step 1: Create `tools/capture_readme_media.ps1`**

The script must:

- Accept `-Manifest tools/readme_media_manifest.json`, `-ProjectNumber`, `-All`, `-SkipGif`, and `-KeepWindows` parameters.
- Read `tools/readme_media_manifest.json`.
- Launch each executable from `Dx11/bin`.
- Wait for the manifest `delayMs`.
- Bring the process main window to the foreground.
- Capture the window rectangle using `System.Drawing.Graphics.CopyFromScreen`.
- Save PNG files under `docs/media/readme/`.
- For project 36, capture frame PNGs for the configured duration and run `C:\ffmpeg\bin\ffmpeg.exe` to encode `docs/media/readme/36-advanced-anim-sound-click.gif`.
- Write `docs/media/readme/capture-report.md` with project number, exe, output file, success/failure, and notes.
- Stop only the process that the script launched, unless `-KeepWindows` is set.

Use these PowerShell building blocks in the implementation:

```powershell
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class ReadmeCaptureWin32 {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
}
"@

function Capture-WindowPng {
  param([System.Diagnostics.Process]$Process, [string]$OutputPath)
  $handle = $Process.MainWindowHandle
  if ($handle -eq [IntPtr]::Zero) { throw "Process has no main window: $($Process.ProcessName)" }
  [ReadmeCaptureWin32]::ShowWindow($handle, 5) | Out-Null
  [ReadmeCaptureWin32]::SetForegroundWindow($handle) | Out-Null
  Start-Sleep -Milliseconds 300
  $rect = New-Object ReadmeCaptureWin32+RECT
  if (-not [ReadmeCaptureWin32]::GetWindowRect($handle, [ref]$rect)) { throw "GetWindowRect failed" }
  $width = $rect.Right - $rect.Left
  $height = $rect.Bottom - $rect.Top
  if ($width -le 32 -or $height -le 32) { throw "Window rectangle too small: ${width}x${height}" }
  $bitmap = New-Object System.Drawing.Bitmap($width, $height)
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
  $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
  $graphics.Dispose()
  $bitmap.Dispose()
}
```

Expected: script exists and contains the `Capture-WindowPng` function.

- [ ] **Step 2: Create `tools/verify_readme_media.ps1`**

The script must:

- Parse the manifest and assert 37 PNGs exist.
- Assert project 36 GIF exists.
- Parse `README.md` for `docs/media/readme/` links.
- Assert no root README project thumbnail uses `github.com/user-attachments`.
- Assert `README_old.md` exists.
- Assert media files are nonzero bytes.
- Print a concise pass/fail summary.

Use this exact verification behavior:

```powershell
$manifest = Get-Content -Raw tools\readme_media_manifest.json | ConvertFrom-Json
$missing = New-Object System.Collections.Generic.List[string]
foreach ($project in $manifest.projects) {
  $image = Join-Path $manifest.mediaDir $project.image
  if (-not (Test-Path -LiteralPath $image)) { $missing.Add($image) }
  elseif ((Get-Item -LiteralPath $image).Length -le 0) { $missing.Add("$image is empty") }
}
$gifProject = $manifest.projects | Where-Object { $_.gif }
$gifPath = Join-Path $manifest.mediaDir $gifProject.gif
if (-not (Test-Path -LiteralPath $gifPath)) { $missing.Add($gifPath) }
elseif ((Get-Item -LiteralPath $gifPath).Length -le 0) { $missing.Add("$gifPath is empty") }
if (-not (Test-Path -LiteralPath README_old.md)) { $missing.Add("README_old.md") }
$readme = Get-Content -Raw README.md
if ($readme -match 'github\.com/user-attachments') { $missing.Add("README.md still references github.com/user-attachments") }
foreach ($project in $manifest.projects) {
  if ($readme -notmatch [regex]::Escape("docs/media/readme/$($project.image)")) {
    $missing.Add("README.md missing docs/media/readme/$($project.image)")
  }
}
if ($readme -notmatch [regex]::Escape("docs/media/readme/$($gifProject.gif)")) {
  $missing.Add("README.md missing docs/media/readme/$($gifProject.gif)")
}
if ($missing.Count -gt 0) {
  $missing | ForEach-Object { Write-Error $_ }
  exit 1
}
"README media verification passed"
```

Expected: verification script exists.

- [ ] **Step 3: Run syntax checks**

Run:

```powershell
$null = [System.Management.Automation.PSParser]::Tokenize((Get-Content -Raw tools\capture_readme_media.ps1), [ref]$null)
$null = [System.Management.Automation.PSParser]::Tokenize((Get-Content -Raw tools\verify_readme_media.ps1), [ref]$null)
"PowerShell scripts parse"
```

Expected: `PowerShell scripts parse`.

- [ ] **Step 4: Commit tooling**

Run:

```powershell
git add tools/capture_readme_media.ps1 tools/verify_readme_media.ps1
git commit -m "chore: add readme media capture tooling"
```

Expected: commit succeeds.

---

### Task 3: Preserve Legacy README

**Files:**
- Create: `README_old.md`
- Modify: `README.md`

- [ ] **Step 1: Copy current README**

Run:

```powershell
Copy-Item -LiteralPath README.md -Destination README_old.md -Force
git diff --no-index -- README.md README_old.md; if ($LASTEXITCODE -eq 1) { throw "README_old.md differs from README.md" }
"README_old.md matches README.md"
```

Expected: `README_old.md matches README.md`.

- [ ] **Step 2: Add a short legacy note to the new README top**

Modify only `README.md`, not `README_old.md`. Insert this sentence directly after the top `# D3D11-AliceTutorial` heading:

```markdown
> 이전 GitHub attachment 기반 README는 [`README_old.md`](README_old.md)에 보존했습니다.
```

Expected: `README.md` has the legacy note and `README_old.md` remains unchanged.

- [ ] **Step 3: Commit legacy preservation**

Run:

```powershell
git add README.md README_old.md
git commit -m "docs: preserve legacy readme"
```

Expected: commit succeeds.

---

### Task 4: Build All README Sample Targets

**Files:**
- No source files changed in this task.

- [ ] **Step 1: Build all root README projects**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\Dx11\TutorialApp.sln /m /t:"01_RenderingQuadangle;02_RenderingCube;03_RenderingMeshAndSceneGraph;04_RenderingMeshWithTexture;05_Mesh;06_pmx;07_pmxTexture;08_ImguiSystemInfo;09_Lighting;10_StaticCube_SkyBox;11_Live2D;12_Lighting_BlinnPhong;13_LineRenderer_AxisDebug;14_Lighting_Phong;15_pmxWithPhong;16_NormalMapping;17_fbx_pmx_obj_WithPhong;18_fbx_Animation;19_MultiModels;20_Depth_And_Alpha_Issue;21_MultiModels_With_Animations;22_VMD;23_Rigid_Animation;24_Skinned_With_Bone_Structure;25_ToonShading_Outline;26_ShadowMap_PCF;27_DebugDraw;28_Scene_Shared3DModel_Animation;29_MousePicking;30_PBR_BRDF;31_IBL;32_Sound_FMOD;33_Sound_Animation_Camera_Motion;34_ToneMapping;35_DeferredRendering;36_AdvancedAnim_Sound_Click;37_Blueprint" /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

Expected: exit code `0`. Existing warning categories such as `C4099` are acceptable if no project fails.

- [ ] **Step 2: Verify executables exist**

Run:

```powershell
$manifest = Get-Content -Raw tools\readme_media_manifest.json | ConvertFrom-Json
foreach ($project in $manifest.projects) {
  $exe = Join-Path $manifest.runtimeDir $project.exe
  if (-not (Test-Path -LiteralPath $exe)) { throw "Missing exe: $exe" }
}
"All manifest executables exist"
```

Expected: `All manifest executables exist`.

---

### Task 5: Capture First Pass Media

**Files:**
- Create: `docs/media/readme/*.png`
- Create: `docs/media/readme/36-advanced-anim-sound-click.gif`
- Create: `docs/media/readme/capture-report.md`

- [ ] **Step 1: Run automated capture**

Run:

```powershell
.\tools\capture_readme_media.ps1 -All
```

Expected:
- `docs/media/readme/01-RenderingQuadangle.png` through `docs/media/readme/37-Blueprint.png` exist.
- `docs/media/readme/36-advanced-anim-sound-click.gif` exists.
- `docs/media/readme/capture-report.md` lists every project.

- [ ] **Step 2: Verify generated media file count**

Run:

```powershell
$pngs = Get-ChildItem docs\media\readme -Filter *.png
$gifs = Get-ChildItem docs\media\readme -Filter *.gif
if ($pngs.Count -ne 37) { throw "Expected 37 png files, got $($pngs.Count)" }
if ($gifs.Count -lt 1) { throw "Expected at least 1 gif file" }
"Media count OK"
```

Expected: `Media count OK`.

- [ ] **Step 3: Commit first pass media only if every image is visible**

Open several representative media files with `view_image` or a local viewer:
- `docs/media/readme/02-RenderingCube.png`
- `docs/media/readme/06-pmx.png`
- `docs/media/readme/25-ToonShading-Outline.png`
- `docs/media/readme/36-AdvancedAnim-Sound-Click.png`
- `docs/media/readme/36-advanced-anim-sound-click.gif`

If these are nonblank and the capture report has no failed rows, run:

```powershell
git add docs/media/readme
git commit -m "docs: capture readme preview media"
```

Expected: commit succeeds.

---

### Task 6: Adjust Startup Camera And Object Framing

**Files:**
- Modify only the startup transform files listed in the File Structure section whose screenshots are blank, clipped, too far away, or dominated by an irrelevant panel.

- [ ] **Step 1: Write a capture audit file**

Create or update `docs/media/readme/capture-report.md` with an `Audit` column containing one of these values for each project:

```markdown
| Project | Image | Audit |
|---|---|---|
| 02_RenderingCube | docs/media/readme/02-RenderingCube.png | pass |
```

Allowed audit values are exactly `pass`, `camera`, `object`, `runtime`, and `manual`.

Expected: every project has one audit value.

- [ ] **Step 2: Apply transform-only fixes**

For rows marked `camera`, adjust only existing startup camera position, rotation, FOV, or speed values in that sample.

Preferred default framing values when the sample uses a free camera:

```cpp
m_Camera.SetPosition(DirectX::XMFLOAT3(0.0f, 3.0f, -7.0f));
m_Camera.SetRotation(DirectX::XMFLOAT3(18.0f, 0.0f, 0.0f));
m_Camera.SetFrustum(DirectX::XMConvertToRadians(60.0f), AspectRatio(), 0.1f, 1000.0f);
```

For samples using `m_CameraPos`, prefer:

```cpp
DirectX::XMFLOAT3 m_CameraPos = { 0.0f, 3.0f, -7.0f };
float m_CameraFovDeg = 60.0f;
float m_CameraNear = 0.1f;
float m_CameraFar = 1000.0f;
```

Expected: source changes are limited to startup transform constants or existing object positions/scales.

- [ ] **Step 3: Apply object placement fixes**

For rows marked `object`, adjust only default object transforms.

Preferred cube/object defaults:

```cpp
DirectX::XMFLOAT3 m_cubePos = { 0.0f, 0.0f, 0.0f };
DirectX::XMFLOAT3 m_cubeScale = { 1.6f, 1.6f, 1.6f };
DirectX::XMFLOAT3 m_cubeRotation = { 20.0f, 35.0f, 0.0f };
```

For project 36's representative scene, keep the public player near world origin and place the two cubes offset from the character:

```cpp
co->cubeTransform.pos = XMFLOAT3(-85.0f, 18.0f, 95.0f);
co->cubeTransform.scale = XMFLOAT3(24.0f, 24.0f, 24.0f);
co2->cubeTransform.pos = XMFLOAT3(85.0f, 18.0f, 95.0f);
co2->cubeTransform.scale = XMFLOAT3(24.0f, 24.0f, 24.0f);
m_Camera.SetPosition(XMFLOAT3(0.0f, 95.0f, -185.0f));
m_Camera.SetRotation(XMFLOAT3(22.0f, 0.0f, 0.0f));
```

Expected: no gameplay logic changes are introduced.

- [ ] **Step 4: Rebuild changed targets**

Run MSBuild for only the projects whose startup transforms changed, using this pattern:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\Dx11\TutorialApp.sln /m /t:"02_RenderingCube;36_AdvancedAnim_Sound_Click" /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

Replace the target list with the actual changed project names.

Expected: exit code `0`.

- [ ] **Step 5: Recapture changed projects**

Run:

```powershell
.\tools\capture_readme_media.ps1 -ProjectNumber 02
.\tools\capture_readme_media.ps1 -ProjectNumber 36
```

Replace the project numbers with the actual changed project numbers.

Expected: recaptured images are visibly framed.

- [ ] **Step 6: Commit framing fixes**

Run:

```powershell
git add Dx11 docs/media/readme
git commit -m "docs: improve readme capture framing"
```

Expected: commit succeeds. If no source transforms changed, skip this commit and note that first pass captures were already usable.

---

### Task 7: Refresh README Media Links

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Replace representative demo media**

In `README.md`, update the representative demo section to include the project 36 GIF:

```markdown
| 36_AdvancedAnim_Sound_Click |
|---|
| <div align="center"><img src="docs/media/readme/36-advanced-anim-sound-click.gif" width="650"/><br/>대표 통합 데모</div> |
```

Expected: the GIF appears before the project shortcut grid.

- [ ] **Step 2: Replace top external attachment images**

The top Youtube/Velog table currently uses GitHub attachment images. Replace that visual table with plain text links so the new README has no `github.com/user-attachments` media dependency:

```markdown
| Youtube | Velog |
|---|---|
| [DirectX11 재생목록](https://www.youtube.com/playlist?list=PLbPdrhrt0AJgCSKYyzjAjHwpQ_Yt4uBMx) | [DirectX11 정리 글](https://velog.io/@whoamicj/series/DirectX11) |
```

Expected: the top table still links to Youtube and Velog but contains no `<img>` tag.

- [ ] **Step 3: Replace every project grid thumbnail**

For each manifest entry, replace the existing GitHub attachment thumbnail with the corresponding repo-local path.

Example for project 02:

```markdown
<img src="docs/media/readme/02-RenderingCube.png" width="200"/>
```

Use these widths:
- `650` for the representative GIF.
- `220` for project grid PNG thumbnails.

Expected: all 37 project entries reference their manifest PNG.

- [ ] **Step 4: Keep existing link destinations**

Do not change the project directory links. They must continue pointing at:

```markdown
https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/02_RenderingCube
```

Expected: clicking a thumbnail still navigates to the matching project directory.

- [ ] **Step 5: Commit README update**

Run:

```powershell
git add README.md
git commit -m "docs: refresh readme preview links"
```

Expected: commit succeeds.

---

### Task 8: Verify README Media And Restricted Asset Safety

**Files:**
- No source files changed in this task.

- [ ] **Step 1: Run README media verifier**

Run:

```powershell
.\tools\verify_readme_media.ps1
```

Expected: `README media verification passed`.

- [ ] **Step 2: Search for restricted asset references**

Run:

```powershell
rg -n "Nikke-Alice|alice-Apose\.pmx|Resource/Nikke-Alice|Study\\char\\char|Alice\.fbm|SkinningTest\.fbx|Study\\char\.fbx|Study\\char\\char\.fbx|alice_normal_mapping_idle_walk_run\.fbx|Rapi\.fbx|Neon\.fbx|Alice_UmaUma|Alice_\.fbx|alice_rabbit|Alice3DGame|AliceDagwa|Waitforsecond|test\.wav|github\.com/user-attachments" README.md docs/media/readme Dx11 --glob '!third_party/**' --glob '!Resource/fbx/Public/MyAlice/**'; if ($LASTEXITCODE -eq 1) { 'NO_RESTRICTED_README_MATCHES' } else { exit $LASTEXITCODE }
```

Expected: `NO_RESTRICTED_README_MATCHES`.

- [ ] **Step 3: Verify old README is the only expected legacy holder**

Run:

```powershell
if (-not (Test-Path README_old.md)) { throw "README_old.md missing" }
rg -n "github\.com/user-attachments" README_old.md
"README_old.md keeps legacy attachment references"
```

Expected: `README_old.md keeps legacy attachment references`.

- [ ] **Step 4: Run diff whitespace check**

Run:

```powershell
git diff --check
```

Expected: exit code `0`.

---

### Task 9: Final Build Verification

**Files:**
- No source files changed in this task.

- [ ] **Step 1: Rebuild all README project targets**

Run the same MSBuild command from Task 4:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\Dx11\TutorialApp.sln /m /t:"01_RenderingQuadangle;02_RenderingCube;03_RenderingMeshAndSceneGraph;04_RenderingMeshWithTexture;05_Mesh;06_pmx;07_pmxTexture;08_ImguiSystemInfo;09_Lighting;10_StaticCube_SkyBox;11_Live2D;12_Lighting_BlinnPhong;13_LineRenderer_AxisDebug;14_Lighting_Phong;15_pmxWithPhong;16_NormalMapping;17_fbx_pmx_obj_WithPhong;18_fbx_Animation;19_MultiModels;20_Depth_And_Alpha_Issue;21_MultiModels_With_Animations;22_VMD;23_Rigid_Animation;24_Skinned_With_Bone_Structure;25_ToonShading_Outline;26_ShadowMap_PCF;27_DebugDraw;28_Scene_Shared3DModel_Animation;29_MousePicking;30_PBR_BRDF;31_IBL;32_Sound_FMOD;33_Sound_Animation_Camera_Motion;34_ToneMapping;35_DeferredRendering;36_AdvancedAnim_Sound_Click;37_Blueprint" /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

Expected: exit code `0`.

- [ ] **Step 2: Confirm git status**

Run:

```powershell
git status --short --branch
```

Expected: branch is `codex/readme-capture-refresh` with no unstaged changes after all final commits.

---

### Task 10: Final Review And Report

**Files:**
- No source files changed in this task.

- [ ] **Step 1: Review final diff summary**

Run:

```powershell
git diff --stat main...HEAD
git log --oneline main..HEAD
```

Expected: diff includes README preservation, capture tooling, media files, README updates, and transform fixes only.

- [ ] **Step 2: Prepare completion summary**

The final report must include:

- Branch name.
- Commit list.
- Media count: 37 PNGs and 1 GIF.
- Build command result.
- README media verification result.
- Restricted asset search result.
- Any project that required manual capture or special handling.

Expected: summary is concise and evidence-backed.
