# Project 38 Stylized Toon-PBR Showcase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add `38_StylizedToonPBR` as a focused, polished Alice character-shading showcase; remove detailed-README logos durably; and replace Projects 01-35 character stills that show side/rear views with deterministic front-facing captures.

**Architecture:** Build Project 38 as a small standalone `GameApp` using the existing `FbxModel`/`AssetManager` path and public `SampleModel.glb`. Render shadow, MRT character color/normal, screen-space outline, and tone mapping as explicit passes. Compare baseline PBR and material-aware Toon-PBR with identical scene inputs. Measure passes with a nonblocking D3D11 timestamp-query ring. Keep every legacy pose correction behind `DX11_README_CAPTURE`.

**Tech Stack:** C++20, Direct3D 11, HLSL Shader Model 5, Assimp-backed `FbxModel`, DirectXTK, ImGui, PowerShell 7, MSBuild, ffmpeg/ffprobe, Git.

**Approved design:** `docs/superpowers/specs/2026-08-21-project38-stylized-toon-pbr-design.md`

## Global constraints

- Use only `Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb`; add no external character asset.
- Do not copy game-specific shader constants, UI, logos, screenshots, names, or textures.
- Keep Project 36's normal runtime intact and preserve its current uncommitted Toon-PBR/media work.
- Keep legacy application behavior unchanged outside `ReadmeCapture::IsEnabled()` branches.
- Stage captured media under ignored `.superpowers/artifacts/` paths and publish only after automated and original-resolution visual checks pass.
- Do not reintroduce `README-BRAND` markers or the detailed-README mascot image.
- Do not touch the main worktree's existing `SampleModel.glb` modification or `.superpowers/` content.
- Commit only explicit task file sets. Before every commit, run `git diff --cached --check` and abort on a nonzero exit.
- Use normal non-force push to `origin/codex/project36-portfolio-showcase` only after fresh verification and review.

## Project 38 file structure

### New application files

- `Dx11/38_StylizedToonPBR/38_StylizedToonPBR.vcxproj`
- `Dx11/38_StylizedToonPBR/38_StylizedToonPBR.vcxproj.filters`
- `Dx11/38_StylizedToonPBR/38_Shared.fxh`
- `Dx11/38_StylizedToonPBR/38_CharacterVS.hlsl`
- `Dx11/38_StylizedToonPBR/38_CharacterPS.hlsl`
- `Dx11/38_StylizedToonPBR/38_FullscreenVS.hlsl`
- `Dx11/38_StylizedToonPBR/38_OutlinePS.hlsl`
- `Dx11/38_StylizedToonPBR/38_ToneMapPS.hlsl`
- `Dx11/38_StylizedToonPBR/App.h`
- `Dx11/38_StylizedToonPBR/App.cpp`
- `Dx11/38_StylizedToonPBR/GpuProfiler.h`
- `Dx11/38_StylizedToonPBR/GpuProfiler.cpp`
- `Dx11/38_StylizedToonPBR/WinMain.cpp`
- `Dx11/38_StylizedToonPBR/README.md`

### New tests

- `tools/tests/test_project38_stylized_toon_pbr.ps1`
- `tools/tests/test_readme_front_facing_capture.ps1`

### New public media

- `docs/media/readme/38-StylizedToonPBR.png`
- `docs/media/readme/38-StylizedToonPBR.gif`
- `docs/media/readme/info/38-StylizedToonPBR-info.png`

---

## Task 1: Verify and checkpoint the pending Project 36 Toon-PBR work

**Files:**

- Modify/commit existing: `Dx11/36_AdvancedAnim_Sound_Click/36_BasicPS.hlsl`
- Modify/commit existing: `Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl`
- Modify/commit existing: `tools/tests/test_project36_portfolio_showcase.ps1`
- Modify/commit existing: `docs/media/readme/36-AdvancedAnim-Sound-Click.png`
- Modify/commit existing: `docs/media/readme/36-advanced-anim-sound-click.gif`
- Modify/commit existing: `docs/media/readme/capture-report.md`

- [ ] **Step 1: Inspect the exact inherited diff**

Run:

```powershell
git diff -- Dx11/36_AdvancedAnim_Sound_Click/36_BasicPS.hlsl Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl tools/tests/test_project36_portfolio_showcase.ps1 docs/media/readme/capture-report.md
git diff --stat -- docs/media/readme/36-AdvancedAnim-Sound-Click.png docs/media/readme/36-advanced-anim-sound-click.gif
```

Confirm the code diff is limited to the approved warm/cool Toon-PBR palette, the current public `SampleModel`, and its source contract. Do not fold any Project 38 work into this commit.

- [ ] **Step 2: Run fresh focused verification**

```powershell
pwsh -NoProfile -File tools/tests/test_project36_portfolio_showcase.ps1
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1
& 'C:\ffmpeg\bin\ffprobe.exe' -v error -count_frames -select_streams v:0 -show_entries stream=width,height,nb_read_frames:format=duration -of default=noprint_wrappers=1 'docs/media/readme/36-advanced-anim-sound-click.gif'
pwsh -NoProfile -File tools/verify_readme_media.ps1
```

Expected: the contract and build exit 0; the Project 36 GIF remains 800x450, 104 frames, and approximately 13 seconds; the public media verifier exits 0.

- [ ] **Step 3: Commit only the Project 36 checkpoint**

```powershell
git add -- Dx11/36_AdvancedAnim_Sound_Click/36_BasicPS.hlsl Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl tools/tests/test_project36_portfolio_showcase.ps1 docs/media/readme/36-AdvancedAnim-Sound-Click.png docs/media/readme/36-advanced-anim-sound-click.gif docs/media/readme/capture-report.md
git diff --cached --check
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
git commit -m "feat: polish project 36 toon pbr showcase"
```

---

## Task 2: Register Project 38 and expand repository-count contracts

**Files:**

- Create: `tools/tests/test_project38_stylized_toon_pbr.ps1`
- Create: Project 38 `.vcxproj`, `.filters`, `WinMain.cpp`, `App.h`, and a minimal `App.cpp`
- Modify: `Dx11/TutorialApp.sln`
- Modify: `Dx11/Directory.Build.targets`
- Modify: `tools/readme_media_manifest.json`
- Modify: `tools/update_readme_branding.ps1`
- Modify: `tools/tests/test_readme_media_manifest.ps1`
- Modify: `tools/tests/test_app_branding.ps1`
- Modify: `tools/tests/test_app_branding_acceptance.ps1`
- Modify: `tools/tests/test_update_readme_branding.ps1`
- Modify: `tools/tests/test_app_icon_resource.ps1`
- Modify: `tools/tests/test_built_app_icons.ps1`

- [ ] **Step 1: Write the failing Project 38 structure contract**

The new PowerShell test must assert:

```powershell
$projectName = '38_StylizedToonPBR'
$projectGuid = '{4B6A5522-0C57-41E0-A222-6AA813BBCE5C}'
Assert-True ($solutionProjectNames.Count -eq 38) 'solution must contain 38 applications'
Assert-True ($solutionText -match [regex]::Escape($projectName)) 'Project 38 missing from solution'
Assert-True ([int]$manifest.expectedProjectCount -eq 38) 'manifest count must be 38'
Assert-True (@($manifest.projects | Where-Object number -eq '38').Count -eq 1) 'manifest Project 38 entry missing'
Assert-True ($targetsText -match ';38_StylizedToonPBR;') 'Project 38 app icon allowlist entry missing'
```

Also require a `Common.vcxproj` project reference, C++20, x64 Debug, the fixed GUID above, all declared Project 38 source/shader files, and a logo-free Project 38 README path.

- [ ] **Step 2: Run the contract and existing count tests to verify RED**

```powershell
pwsh -NoProfile -File tools/tests/test_project38_stylized_toon_pbr.ps1
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
pwsh -NoProfile -File tools/tests/test_app_icon_resource.ps1
pwsh -NoProfile -File tools/tests/test_app_branding.ps1
```

Expected: failure because Project 38 and the 38-project count do not exist.

- [ ] **Step 3: Add the minimal application and solution entry**

Create a standard `GameApp` executable with `WinMain.cpp`, D3D11 device/swap-chain/depth initialization, ImGui initialization, a clear frame, resize-safe cleanup, and a visible initialization error if setup fails. Do not add rendering features yet.

Register the fixed GUID in `TutorialApp.sln` and add all Debug/Release Win32/x64 configuration mappings. Add `38_StylizedToonPBR` to `AliceTutorialBrandingProjects`.

The `.vcxproj` references `..\Common\Common.vcxproj`, uses C++20 for x64 Debug, copies the public `Resource` directory for runtime loading, and lists runtime HLSL source files through `CopyFileToFolders`/`None` so `Dx11/Directory.Build.targets` publishes them to `Dx11/bin`.

- [ ] **Step 4: Add the Project 38 manifest entry and update every count fixture**

Append:

```json
{"number":"38","name":"Stylized Toon PBR","directory":"38_StylizedToonPBR","exe":"38_StylizedToonPBR.exe","image":"38-StylizedToonPBR.png","gif":"38-StylizedToonPBR.gif","infoImage":"info/38-StylizedToonPBR-info.png","delayMs":3000,"readmeCaptureMode":true,"title":"Stylized Toon PBR","summary":"재질별 Hybrid Toon-PBR와 외곽선 비용을 비교합니다.","tags":["Toon PBR","Outline","GPU Profiling","Character"],"gifPhase":"runtime"}
```

Change production and fixture assumptions from 37 to 38 in the files listed above. Keep the presentation-pan allowlist unchanged because Project 38 supplies its own meaningful runtime motion.

- [ ] **Step 5: Run structure/count tests and build the empty shell**

```powershell
$tests = @(
  'tools/tests/test_project38_stylized_toon_pbr.ps1',
  'tools/tests/test_readme_media_manifest.ps1',
  'tools/tests/test_app_icon_resource.ps1',
  'tools/tests/test_app_branding.ps1',
  'tools/tests/test_app_branding_acceptance.ps1',
  'tools/tests/test_update_readme_branding.ps1'
)
foreach ($test in $tests) { pwsh -NoProfile -File $test; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } }
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\38_StylizedToonPBR\38_StylizedToonPBR.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
```

Expected: all tests and the Project 38 shell build exit 0; the icon resource test recognizes 38 apps.

- [ ] **Step 6: Commit the registered shell**

```powershell
git add -- Dx11/TutorialApp.sln Dx11/Directory.Build.targets Dx11/38_StylizedToonPBR tools/readme_media_manifest.json tools/update_readme_branding.ps1 tools/tests/test_project38_stylized_toon_pbr.ps1 tools/tests/test_readme_media_manifest.ps1 tools/tests/test_app_branding.ps1 tools/tests/test_app_branding_acceptance.ps1 tools/tests/test_update_readme_branding.ps1 tools/tests/test_app_icon_resource.ps1 tools/tests/test_built_app_icons.ps1
git diff --cached --check
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
git commit -m "build: register project 38 stylized toon pbr"
```

---

## Task 3: Implement a nonblocking D3D11 GPU timing ring

**Files:**

- Create: `Dx11/38_StylizedToonPBR/GpuProfiler.h`
- Create: `Dx11/38_StylizedToonPBR/GpuProfiler.cpp`
- Modify: `Dx11/38_StylizedToonPBR/38_StylizedToonPBR.vcxproj`
- Modify: `Dx11/38_StylizedToonPBR/38_StylizedToonPBR.vcxproj.filters`
- Modify: `tools/tests/test_project38_stylized_toon_pbr.ps1`

- [ ] **Step 1: Extend the contract and verify RED**

Require `D3D11_QUERY_TIMESTAMP_DISJOINT`, start/end timestamp queries for `Shadow`, `Character`, `Outline`, and `ToneMap`, a ring size of at least three frames, and `D3D11_ASYNC_GETDATA_DONOTFLUSH`. Reject any `while` loop around `GetData`.

```powershell
pwsh -NoProfile -File tools/tests/test_project38_stylized_toon_pbr.ps1
```

Expected: failure because the profiler does not exist.

- [ ] **Step 2: Implement `GpuProfiler`**

Expose:

```cpp
enum class GpuPass : uint8_t { Shadow, Character, Outline, ToneMap, Count };
struct GpuTimings { bool valid; double totalMs; std::array<double, 4> passMs; };

bool Initialize(ID3D11Device* device);
void BeginFrame(ID3D11DeviceContext* context);
void BeginPass(ID3D11DeviceContext* context, GpuPass pass);
void EndPass(ID3D11DeviceContext* context, GpuPass pass);
void EndFrame(ID3D11DeviceContext* context);
void Resolve(ID3D11DeviceContext* context);
const GpuTimings& Latest() const;
```

Use four query slots. Resolve only an older slot. Every `GetData` call uses `D3D11_ASYNC_GETDATA_DONOTFLUSH`; if any result is not ready, return immediately and preserve the last valid timings. Convert ticks using the matching disjoint frequency and discard disjoint samples.

- [ ] **Step 3: Run the focused test and rebuild**

```powershell
pwsh -NoProfile -File tools/tests/test_project38_stylized_toon_pbr.ps1
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\38_StylizedToonPBR\38_StylizedToonPBR.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1
```

- [ ] **Step 4: Commit the profiler**

```powershell
git add -- Dx11/38_StylizedToonPBR/GpuProfiler.h Dx11/38_StylizedToonPBR/GpuProfiler.cpp Dx11/38_StylizedToonPBR/38_StylizedToonPBR.vcxproj Dx11/38_StylizedToonPBR/38_StylizedToonPBR.vcxproj.filters tools/tests/test_project38_stylized_toon_pbr.ps1
git diff --cached --check
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
git commit -m "feat: add nonblocking gpu timing ring"
```

---

## Task 4: Implement the Hybrid Toon-PBR renderer and compact HUD

**Files:**

- Create: all Project 38 HLSL/FXH files listed in the file structure
- Modify: `Dx11/38_StylizedToonPBR/App.h`
- Modify: `Dx11/38_StylizedToonPBR/App.cpp`
- Modify: Project 38 `.vcxproj` and `.filters`
- Modify: `tools/tests/test_project38_stylized_toon_pbr.ps1`

- [ ] **Step 1: Extend the source contract and verify RED**

Require these observable contracts:

- the app loads only `..\Resource\fbx\Public\MyAlice\Player\SampleModel.glb`;
- render modes `Pbr`, `ToonPbr`, and `Split` exist;
- presets `NeonContrast` and `IndustrialSoft` exist;
- the character pixel shader emits HDR color and encoded world normal to two render targets;
- the toon path contains diffuse band thresholds/softness, warm/cool tint, material profile, hair band, rim term, and alpha clip;
- the outline shader samples both normal and depth and supports two quality levels;
- the tone-map shader includes the selected preset/exposure and composites a material-aware outline color;
- the HUD contains the exact approved two-line description;
- Project 38 calls `GpuProfiler` around all four measured passes.

Run the test and expect RED before writing shaders.

- [ ] **Step 2: Create resize-safe render resources**

In `App`, own these resources with `Microsoft::WRL::ComPtr` or explicit release helpers:

- HDR color: `DXGI_FORMAT_R16G16B16A16_FLOAT`, RTV + SRV;
- encoded normal/profile: `DXGI_FORMAT_R16G16B16A16_FLOAT`, RTV + SRV;
- main depth: `DXGI_FORMAT_R32_TYPELESS`, `D32_FLOAT` DSV + `R32_FLOAT` SRV;
- outline mask: `DXGI_FORMAT_R8_UNORM`, RTV + SRV;
- shadow map: `DXGI_FORMAT_R32_TYPELESS`, `D32_FLOAT` DSV + `R32_FLOAT` SRV;
- backbuffer RTV and one fullscreen-triangle pipeline.

Recreate window-sized resources only when client dimensions change. Unbind SRVs before rebinding the same resources as RTV/DSV.

- [ ] **Step 3: Load and animate the existing model**

Load through `AssetManager::GetInstance().GetFbxModel`. Fail initialization with a clear message if the model or mesh is missing. Bind the `VertexSkinnedTBN` input layout:

```cpp
POSITION R32G32B32_FLOAT
NORMAL R32G32B32_FLOAT
TANGENT R32G32B32_FLOAT
BINORMAL R32G32B32_FLOAT
COLOR R32G32B32A32_FLOAT
TEXCOORD R32G32_FLOAT
BLENDINDICES R16G16B16A16_UINT
BLENDWEIGHT R32G32B32A32_FLOAT
```

Use `FbxModel::UpdateAnimation` and its bone constant buffer. Keep yaw fixed near a front three-quarter angle; animate only the existing idle clip. In README capture mode, freeze the yaw and use a deterministic time/pose.

Build one material-profile record per Assimp material. Prefer case-insensitive name matches for `hair`, `face`/`skin`, and cloth/body terms, then fall back to cloth. Keep an explicit per-material override table in Project 38 so classification is deterministic for `SampleModel`.

- [ ] **Step 4: Implement measured render passes**

1. Shadow pass renders the skinned character into the shadow DSV.
2. Character pass renders to HDR + normal/profile MRT with the selected mode.
3. Outline pass renders a fullscreen triangle to the R8 mask using depth/normal discontinuities and resolution-scaled offsets.
4. Tone-map pass renders a fullscreen triangle to the backbuffer and composites the outline/background.

For split mode, render left PBR and right Toon-PBR using half-width viewports/scissors and the same pose, camera, light, exposure, and targets. Restore full-screen state before outline and tone mapping.

- [ ] **Step 5: Implement original presets and the compact HUD**

Default to Toon-PBR + `Neon Contrast`. Provide keyboard and ImGui controls for mode, preset, band thresholds/softness, shadow tint, key tint, hair highlight, rim, outline width/quality, and exposure.

Keep the description panel concise:

```text
Hybrid Toon-PBR Character Showcase
Material-aware toon shading, hair highlights, rim lighting, stable outlines, and GPU cost comparison.
```

Below it show active mode/preset, CPU milliseconds, total GPU milliseconds, and Shadow/Character/Outline/ToneMap timings. Before a valid sample, show `GPU: warming up`; if profiler initialization fails, show `GPU: unavailable` and continue rendering.

- [ ] **Step 6: Run contract, rebuild, and smoke-run both modes**

```powershell
pwsh -NoProfile -File tools/tests/test_project38_stylized_toon_pbr.ps1
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\38_StylizedToonPBR\38_StylizedToonPBR.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
```

Run the normal executable for at least eight seconds and confirm it has a responsive visible window. Run again with `DX11_README_CAPTURE=1` and confirm a deterministic front three-quarter character, readable HUD, and no shader/resource warnings. Stop only the exact launched PIDs.

- [ ] **Step 7: Commit the renderer**

```powershell
git add -- Dx11/38_StylizedToonPBR tools/tests/test_project38_stylized_toon_pbr.ps1
git diff --cached --check
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
git commit -m "feat: add project 38 hybrid toon pbr renderer"
```

---

## Task 5: Make legacy README character stills deterministic and front-facing

**Files:**

- Create: `tools/tests/test_readme_front_facing_capture.ps1`
- Modify capture-only blocks in:
  - `Dx11/17_fbx_pmx_obj_WithPhong/App.cpp`
  - `Dx11/18_fbx_Animation/App.cpp`
  - `Dx11/19_MultiModels/App.cpp`
  - `Dx11/20_Depth_And_Alpha_Issue/App.cpp`
  - `Dx11/21_MultiModels_With_Animations/App.cpp`
  - `Dx11/22_VMD/App.cpp`
  - `Dx11/24_Skinned_With_Bone_Structure/App.cpp`
  - `Dx11/26_ShadowMap_PCF/App.cpp`
  - `Dx11/27_DebugDraw/App.cpp`
  - `Dx11/28_Scene_Shared3DModel_Animation/App.cpp`
  - `Dx11/29_MousePicking/App.cpp`
  - `Dx11/30_PBR_BRDF/App.cpp`
  - `Dx11/31_IBL/App.cpp`
  - `Dx11/32_Sound_FMOD/App.cpp`
  - `Dx11/33_Sound_Animation_Camera_Motion/App.cpp`

- [ ] **Step 1: Write the failing pose contract**

The test must parse only `ReadmeCapture::IsEnabled()` setup blocks and require:

- Projects 17-18 set `m_RotateModel = false` after the fixed yaw;
- Projects 19-22 and 24 set `autoRotate = false` after the fixed yaw;
- Projects 26-27 use only front-facing capture yaws with absolute values at most 35 degrees;
- Projects 28-29 explicitly set the hero yaw to a front three-quarter value and disable hero auto-rotation;
- Projects 30-33 keep every visible character capture yaw within the same front-facing range;
- normal-mode member defaults and update loops remain present and unchanged.

Run and expect RED because the current capture blocks enable rotation or use rear-facing `±130-180` degree yaws.

- [ ] **Step 2: Apply the minimal capture-only source fix**

Use approximately `-25` degrees for the main hero and small `-20..20` degree variations for visible companions. Do not change animation playback, camera controls, or normal-mode `autoRotate` behavior. For Projects 26-27, preserve the multi-character positions and debug/shadow purpose while rotating all visible character meshes toward the capture camera.

- [ ] **Step 3: Run the test and rebuild every changed application**

```powershell
pwsh -NoProfile -File tools/tests/test_readme_front_facing_capture.ps1
$projects = @('17_fbx_pmx_obj_WithPhong','18_fbx_Animation','19_MultiModels','20_Depth_And_Alpha_Issue','21_MultiModels_With_Animations','22_VMD','24_Skinned_With_Bone_Structure','26_ShadowMap_PCF','27_DebugDraw','28_Scene_Shared3DModel_Animation','29_MousePicking','30_PBR_BRDF','31_IBL','32_Sound_FMOD','33_Sound_Animation_Camera_Motion')
foreach ($project in $projects) {
  & 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' "Dx11\$project\$project.vcxproj" /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
```

- [ ] **Step 4: Commit capture behavior before binary media**

```powershell
git add -- tools/tests/test_readme_front_facing_capture.ps1 Dx11/17_fbx_pmx_obj_WithPhong/App.cpp Dx11/18_fbx_Animation/App.cpp Dx11/19_MultiModels/App.cpp Dx11/20_Depth_And_Alpha_Issue/App.cpp Dx11/21_MultiModels_With_Animations/App.cpp Dx11/22_VMD/App.cpp Dx11/24_Skinned_With_Bone_Structure/App.cpp Dx11/26_ShadowMap_PCF/App.cpp Dx11/27_DebugDraw/App.cpp Dx11/28_Scene_Shared3DModel_Animation/App.cpp Dx11/29_MousePicking/App.cpp Dx11/30_PBR_BRDF/App.cpp Dx11/31_IBL/App.cpp Dx11/32_Sound_FMOD/App.cpp Dx11/33_Sound_Animation_Camera_Motion/App.cpp
git diff --cached --check
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
git commit -m "fix: keep readme character captures front facing"
```

---

## Task 6: Integrate Project 38 documentation and durable logo removal

**Files:**

- Create/finalize: `Dx11/38_StylizedToonPBR/README.md`
- Modify: `Dx11/37_Blueprint/README.md`
- Modify: `README.md`
- Modify: count/branding files from Task 2 as tests require
- Verify all: `Dx11/01_*/README.md` through `Dx11/38_*/README.md`

- [ ] **Step 1: Extend documentation assertions and verify RED**

Extend `test_project38_stylized_toon_pbr.ps1` to require:

- Project 37 next links point to Project 38;
- Project 38 previous links point to Project 37 and its next label is plain `다음`;
- Project 38 README documents PBR/Toon/Split, both presets, material profiles, outline, GPU query latency, controls, and optimization evidence;
- root README contains the Project 38 directory and PNG gallery link;
- none of the 38 project READMEs contains `README-BRAND`, `alice-tutorial-logo.png`, or a centered mascot image block.

Run the focused test and `test_app_branding.ps1`; expect the focused test to fail until docs exist.

- [ ] **Step 2: Write the detailed README and root showcase entry**

Use the standard nav/info/runtime marker structure. Do not add a logo block. Explain what is actually implemented and distinguish measured optimization evidence from general advice. Add Project 38 to the root gallery and a short stylized-rendering showcase section after the existing Project 36 representative demo.

- [ ] **Step 3: Run the idempotent logo remover and inspect its scope**

```powershell
pwsh -NoProfile -File tools/update_readme_branding.ps1
rg -n 'README-BRAND:(START|END)|alice-tutorial-logo\.png' Dx11 -g README.md
```

Expected: `rg` returns no matches. The updater may change only a legacy block if one has reappeared; otherwise it is byte-idempotent.

- [ ] **Step 4: Run documentation/branding tests**

```powershell
$tests = @(
  'tools/tests/test_project38_stylized_toon_pbr.ps1',
  'tools/tests/test_app_branding.ps1',
  'tools/tests/test_app_branding_acceptance.ps1',
  'tools/tests/test_update_readme_branding.ps1',
  'tools/tests/test_project_readme_updater.ps1'
)
foreach ($test in $tests) { pwsh -NoProfile -File $test; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } }
```

- [ ] **Step 5: Commit documentation text**

```powershell
git add -- README.md Dx11/37_Blueprint/README.md Dx11/38_StylizedToonPBR/README.md tools/tests/test_project38_stylized_toon_pbr.ps1 tools/update_readme_branding.ps1 tools/tests/test_app_branding.ps1 tools/tests/test_app_branding_acceptance.ps1 tools/tests/test_update_readme_branding.ps1
git diff --cached --check
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
git commit -m "docs: add project 38 toon pbr guide"
```

---

## Task 7: Capture, visually approve, and publish front-facing legacy stills

**Files:**

- Modify only approved PNGs under `docs/media/readme/` for Projects 15-35
- Modify matching `docs/media/readme/info/*-info.png`
- Modify relevant PNG rows in `docs/media/readme/capture-report.md`

- [ ] **Step 1: Capture every character-loading Project 15-35 still to staging**

Use this audit set, which includes the known failures and neighboring character projects:

```powershell
$artifactDir = '.superpowers/artifacts/front-facing-readme'
$numbers = @('15','16','17','18','19','20','21','22','24','25','26','27','28','29','30','31','32','33','34','35')
foreach ($number in $numbers) {
  pwsh -NoProfile -File tools/capture_readme_media.ps1 -ProjectNumber $number -SkipGif -OutputDir $artifactDir
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
```

Do not publish during this step.

- [ ] **Step 2: Perform original-resolution visual QA**

Inspect every staged PNG, not only thumbnails. Require:

- a visible character's face/front torso is readable;
- no primary character is shown from the rear;
- front three-quarter views are acceptable, pure side silhouettes are not;
- heads, ears, hands, and feet are not accidentally cropped when the tutorial composition intends a full character;
- the intended tutorial evidence (bones, shadows, PBR spheres, debug boxes, audio/UI) remains visible.

If a still fails, change only its capture-mode yaw/camera/position, add or tighten the pose contract, rebuild that project, and recapture it. Do not compensate by shortening the delay.

- [ ] **Step 3: Publish only approved replacements and regenerate matching info images**

Copy each approved staged PNG to its manifest path. For each replaced number run:

```powershell
pwsh -NoProfile -File tools/generate_readme_info_images.ps1 -ProjectNumber $number
```

Merge only the successful PNG rows from the staged reports into `docs/media/readme/capture-report.md`; preserve all GIF rows and unrelated project rows.

- [ ] **Step 4: Generate the repository review sheet and recheck the public files**

```powershell
pwsh -NoProfile -File tools/generate_readme_review_sheets.ps1 -OutputDir '.superpowers/artifacts/front-facing-readme/review'
pwsh -NoProfile -File tools/tests/test_readme_front_facing_capture.ps1
pwsh -NoProfile -File tools/tests/test_readme_info_images.ps1
pwsh -NoProfile -File tools/verify_readme_media.ps1
```

Inspect the public PNG review sheet and spot-check every replaced public PNG at original resolution.

- [ ] **Step 5: Commit only replaced legacy media**

Stage the exact replacement PNGs, their exact derived info images, and `capture-report.md`; inspect `git diff --cached --name-only` before committing.

```powershell
git diff --cached --check
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
git commit -m "docs: refresh front facing character captures"
```

---

## Task 8: Capture and publish the Project 38 showcase media

**Files:**

- Create: `docs/media/readme/38-StylizedToonPBR.png`
- Create: `docs/media/readme/38-StylizedToonPBR.gif`
- Create: `docs/media/readme/info/38-StylizedToonPBR-info.png`
- Modify: `docs/media/readme/capture-report.md`

- [ ] **Step 1: Capture to staging**

```powershell
$artifactDir = '.superpowers/artifacts/project38-stylized-toon-pbr'
pwsh -NoProfile -File tools/capture_readme_media.ps1 -ProjectNumber 38 -OutputDir $artifactDir
```

Expected: 1600x900 PNG, 800x450 GIF, roughly 32 frames/four seconds, and two successful report rows.

- [ ] **Step 2: Verify dimensions, motion, and visual design**

```powershell
& 'C:\ffmpeg\bin\ffprobe.exe' -v error -count_frames -select_streams v:0 -show_entries stream=width,height,nb_read_frames:format=duration -of default=noprint_wrappers=1 "$artifactDir/38-StylizedToonPBR.gif"
& 'C:\ffmpeg\bin\ffmpeg.exe' -y -i "$artifactDir/38-StylizedToonPBR.gif" -vf 'fps=1,scale=800:450,tile=2x2' "$artifactDir/contact-sheet.png"
```

Inspect the staged PNG, GIF, and contact sheet at original resolution. Require a clear face/front three-quarter composition, visible warm/cool tone planes, distinct hair highlight and rim, clean outline without obvious haloing, readable compact HUD, no clipped body parts, and meaningful animation/mode motion. Confirm the visual result is an original premium-anime-inspired rendering, not a game UI imitation.

- [ ] **Step 3: Publish and regenerate the Project 38 info image**

Copy the approved PNG/GIF to their manifest destinations, run:

```powershell
pwsh -NoProfile -File tools/generate_readme_info_images.ps1 -ProjectNumber 38
pwsh -NoProfile -File tools/verify_readme_media.ps1
pwsh -NoProfile -File tools/tests/test_readme_info_images.ps1
```

Merge only the two successful Project 38 rows into `capture-report.md`.

- [ ] **Step 4: Commit Project 38 public media**

```powershell
git add -- docs/media/readme/38-StylizedToonPBR.png docs/media/readme/38-StylizedToonPBR.gif docs/media/readme/info/38-StylizedToonPBR-info.png docs/media/readme/capture-report.md
git diff --cached --check
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
git commit -m "docs: publish project 38 toon pbr showcase"
```

---

## Task 9: Final regression, review, and GitHub push

**Files:** verification only unless a test reveals a scoped defect.

- [ ] **Step 1: Invoke verification-before-completion and run fresh tests**

Run at minimum:

```powershell
$tests = @(
  'tools/tests/test_project38_stylized_toon_pbr.ps1',
  'tools/tests/test_readme_front_facing_capture.ps1',
  'tools/tests/test_project36_portfolio_showcase.ps1',
  'tools/tests/test_readme_media_manifest.ps1',
  'tools/tests/test_app_icon_resource.ps1',
  'tools/tests/test_app_branding.ps1',
  'tools/tests/test_app_branding_acceptance.ps1',
  'tools/tests/test_update_readme_branding.ps1',
  'tools/tests/test_project_readme_updater.ps1',
  'tools/tests/test_readme_info_images.ps1'
)
foreach ($test in $tests) { pwsh -NoProfile -File $test; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } }
pwsh -NoProfile -File tools/capture_readme_media.ps1 -ValidateOnly
pwsh -NoProfile -File tools/verify_readme_media.ps1
```

- [ ] **Step 2: Rebuild and smoke-test final executables**

```powershell
$buildTargets = @('36_AdvancedAnim_Sound_Click','38_StylizedToonPBR')
foreach ($project in $buildTargets) {
  & 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' "Dx11\$project\$project.vcxproj" /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
$builtAfter = (Get-Date).AddMinutes(-10)
pwsh -NoProfile -File tools/tests/test_built_app_icons.ps1 -BinRoot Dx11 -NotOlderThan $builtAfter -ProjectNames 36_AdvancedAnim_Sound_Click,38_StylizedToonPBR
```

Run Project 38 in normal and capture mode and confirm responsive visible windows. Re-run one representative repaired legacy capture (Project 21 or 24) from the final binaries.

- [ ] **Step 3: Inspect repository scope and media one last time**

```powershell
rg -n 'README-BRAND:(START|END)|alice-tutorial-logo\.png' Dx11 -g README.md
git diff --check origin/codex/project36-portfolio-showcase..HEAD
git status --short
git log --oneline --decorate origin/codex/project36-portfolio-showcase..HEAD
```

Expected: no detailed README logo matches; `diff --check` is silent; the worktree has no uncommitted deliverable changes; only explicitly ignored verification artifacts may remain.

- [ ] **Step 4: Invoke requesting-code-review**

Use a fresh reviewer agent as required by the skill. Give it the approved design, implementation plan, base commit `619ed71`, final `HEAD`, and request findings ordered by severity. Fix valid findings with focused tests and repeat the relevant verification.

- [ ] **Step 5: Invoke finishing-a-development-branch and push**

Because the user explicitly requested GitHub delivery, push the already tracked feature branch without force:

```powershell
git push origin codex/project36-portfolio-showcase
```

Verify that `origin/codex/project36-portfolio-showcase` resolves to local `HEAD`. Report the final commit IDs, branch name, build/test/media results, and the exact set of recaptured Project numbers.
