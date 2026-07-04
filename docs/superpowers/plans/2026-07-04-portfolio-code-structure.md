# Portfolio Code Structure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the representative late-stage DX11 demo easier to review as a portfolio by lightly splitting the large `36_AdvancedAnim_Sound_Click/App.cpp` and adding missing architecture documentation.

**Architecture:** Keep `App`, `App::Impl`, public function names, shader files, resource paths, and runtime behavior unchanged. Split the implementation into focused `.inl` include parts so the code remains one translation unit, which preserves access to file-local helper types while giving recruiters clear file names to scan.

**Tech Stack:** C++20, Direct3D 11, Visual Studio/MSBuild project XML, HLSL, Markdown.

---

## File Structure

**Create:**
- `Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl`  
  Owns file-local structs, enums, static helper functions, and `App::Impl`.
- `Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl`  
  Owns constructor/destructor, initialization, shutdown, async loading, and setup/teardown helpers.
- `Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl`  
  Owns input processing, update, and constant buffer update helper.
- `Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl`  
  Owns frame render orchestration and render passes.
- `Dx11/36_AdvancedAnim_Sound_Click/App_ModelLoading.inl`  
  Owns model load/unload functions.
- `Dx11/36_AdvancedAnim_Sound_Click/App_ImGuiPanels.inl`  
  Owns ImGui panels and debug UI.
- `Dx11/36_AdvancedAnim_Sound_Click/App_Utilities.inl`  
  Owns skybox, scene image, scene switching, trim, HDR, and swapchain helpers.
- `Dx11/35_DeferredRendering/README.md`  
  Documents the deferred rendering sample.

**Modify:**
- `Dx11/36_AdvancedAnim_Sound_Click/App.cpp`  
  Reduce to includes, library pragmas, namespace aliases, and ordered `.inl` includes.
- `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj`  
  Register new `.inl` files as project-visible include files.
- `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj.filters`  
  Add a filter for implementation parts.
- `README.md`  
  Add a compact representative demo and code map section near the top.

---

### Task 1: Baseline And Structural Guard

**Files:**
- Read: `Dx11/36_AdvancedAnim_Sound_Click/App.cpp`
- Read: `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj`
- Read: `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj.filters`

- [ ] **Step 1: Confirm the starting file is still the known large App**

Run:

```powershell
(Get-Content -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/App.cpp').Count
rg -n "^\\s*(App::|template <typename T>|void App::|bool App::)" Dx11/36_AdvancedAnim_Sound_Click/App.cpp
```

Expected:

```text
App.cpp is about 6400 lines.
Function boundaries include App::App, OnInitialize, OnUpdate, OnRender, InitD3D, LoadModelFromFile, RenderControlPannel, and CreateSwapChainAndBackBuffer.
```

- [ ] **Step 2: Run the structural check that should fail before the split**

Run:

```powershell
$lineCount = (Get-Content -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/App.cpp').Count
if ($lineCount -lt 120) { throw 'Unexpected: App.cpp is already split.' } else { "Expected pre-split large App.cpp: $lineCount lines" }
```

Expected:

```text
Expected pre-split large App.cpp: <line count> lines
```

- [ ] **Step 3: Commit nothing**

Run:

```powershell
git status --short
```

Expected:

```text
No implementation changes yet.
```

---

### Task 2: Split `36_AdvancedAnim_Sound_Click/App.cpp`

**Files:**
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App.cpp`
- Create: `Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl`
- Create: `Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl`
- Create: `Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl`
- Create: `Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl`
- Create: `Dx11/36_AdvancedAnim_Sound_Click/App_ModelLoading.inl`
- Create: `Dx11/36_AdvancedAnim_Sound_Click/App_ImGuiPanels.inl`
- Create: `Dx11/36_AdvancedAnim_Sound_Click/App_Utilities.inl`

- [ ] **Step 1: Split the file with a mechanical PowerShell script**

Run this script from the repository root. It preserves the original function bodies and moves chunks by known function boundaries.

```powershell
$appPath = 'Dx11/36_AdvancedAnim_Sound_Click/App.cpp'
$lines = Get-Content -LiteralPath $appPath

function Find-LineIndex([string]$pattern) {
    for ($i = 0; $i -lt $lines.Count; ++$i) {
        if ($lines[$i] -match $pattern) { return $i }
    }
    throw "Pattern not found: $pattern"
}

function Slice-Lines([int]$start, [int]$endExclusive) {
    if ($endExclusive -le $start) { return @() }
    return $lines[$start..($endExclusive - 1)]
}

$typeStart = Find-LineIndex '^// 내부 전용 타입들'
$ctorStart = Find-LineIndex '^App::App\(\)'
$skyUtilStart = Find-LineIndex '^void App::PrepareSkyFaceSRVs'
$onInitStart = Find-LineIndex '^bool App::OnInitialize'
$inputStart = Find-LineIndex '^void App::OnInputProcess'
$renderStart = Find-LineIndex '^void App::OnRender'
$initD3DStart = Find-LineIndex '^bool App::InitD3D'
$modelStart = Find-LineIndex '^bool App::LoadModelFromFile'
$imguiStart = Find-LineIndex '^void App::RenderControlPannel'
$tailUtilStart = Find-LineIndex '^void App::LoadSceneImage'

$internal = @()
$internal += Slice-Lines $typeStart $ctorStart
$internal += ''
$internal += Slice-Lines ((Find-LineIndex '^static bool LoadTextureSRVAndSize')) $skyUtilStart

$lifecycle = @()
$lifecycle += Slice-Lines $ctorStart ((Find-LineIndex '^static bool LoadTextureSRVAndSize'))
$lifecycle += ''
$lifecycle += Slice-Lines $onInitStart $inputStart
$lifecycle += ''
$lifecycle += Slice-Lines $initD3DStart $modelStart

$updateInput = Slice-Lines $inputStart $renderStart

$renderPasses = Slice-Lines $renderStart $initD3DStart

$modelLoading = Slice-Lines $modelStart $imguiStart

$imguiPanels = Slice-Lines $imguiStart $tailUtilStart

$utilities = @()
$utilities += Slice-Lines $skyUtilStart $onInitStart
$utilities += ''
$utilities += Slice-Lines $tailUtilStart $lines.Count

Set-Content -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl' -Value $internal
Set-Content -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl' -Value $lifecycle
Set-Content -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl' -Value $updateInput
Set-Content -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl' -Value $renderPasses
Set-Content -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/App_ModelLoading.inl' -Value $modelLoading
Set-Content -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/App_ImGuiPanels.inl' -Value $imguiPanels
Set-Content -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/App_Utilities.inl' -Value $utilities

$index = @()
$index += Slice-Lines 0 $typeStart
$index += ''
$index += '// Implementation is split by responsibility for portfolio readability.'
$index += '// These files are included here so App::Impl and file-local helpers remain private to this translation unit.'
$index += '#include "App_InternalTypes.inl"'
$index += '#include "App_Utilities.inl"'
$index += '#include "App_Lifecycle.inl"'
$index += '#include "App_UpdateInput.inl"'
$index += '#include "App_RenderPasses.inl"'
$index += '#include "App_ModelLoading.inl"'
$index += '#include "App_ImGuiPanels.inl"'
Set-Content -LiteralPath $appPath -Value $index
```

- [ ] **Step 2: Verify split output exists and App.cpp is now an index**

Run:

```powershell
Get-ChildItem -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click' -Filter 'App_*.inl' | Select-Object Name,Length
(Get-Content -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/App.cpp').Count
rg -n "#include \"App_.*\\.inl\"" Dx11/36_AdvancedAnim_Sound_Click/App.cpp
```

Expected:

```text
Seven App_*.inl files are present.
App.cpp is under 120 lines.
App.cpp includes App_InternalTypes.inl through App_ImGuiPanels.inl.
```

- [ ] **Step 3: Check no App member implementation disappeared**

Run:

```powershell
rg -n "^\\s*(App::|template <typename T>|void App::|bool App::)" Dx11/36_AdvancedAnim_Sound_Click/App*.inl
```

Expected:

```text
All previous App functions are present in App_*.inl files.
```

- [ ] **Step 4: Commit the split**

Run:

```powershell
git add -- Dx11/36_AdvancedAnim_Sound_Click/App.cpp Dx11/36_AdvancedAnim_Sound_Click/App_*.inl
git commit -m "refactor: split advanced demo app implementation"
```

Expected:

```text
Commit succeeds with App.cpp and seven App_*.inl files.
```

---

### Task 3: Register The New Implementation Parts In Visual Studio

**Files:**
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj.filters`

- [ ] **Step 1: Add `.inl` files to the project file**

In `36_AdvancedAnim_Sound_Click.vcxproj`, add these entries to the existing `<ItemGroup>` that contains `ClInclude Include="App.h"`:

```xml
    <ClInclude Include="App_InternalTypes.inl" />
    <ClInclude Include="App_Lifecycle.inl" />
    <ClInclude Include="App_UpdateInput.inl" />
    <ClInclude Include="App_RenderPasses.inl" />
    <ClInclude Include="App_ModelLoading.inl" />
    <ClInclude Include="App_ImGuiPanels.inl" />
    <ClInclude Include="App_Utilities.inl" />
```

- [ ] **Step 2: Add a Visual Studio filter**

In `36_AdvancedAnim_Sound_Click.vcxproj.filters`, add this filter inside the first `<ItemGroup>`:

```xml
    <Filter Include="App Parts">
      <UniqueIdentifier>{c1f8b455-428e-44bb-9cfb-0df93773d6f4}</UniqueIdentifier>
      <Extensions>inl</Extensions>
    </Filter>
```

Then add these entries inside the `<ItemGroup>` that contains `ClInclude Include="App.h"`:

```xml
    <ClInclude Include="App_InternalTypes.inl">
      <Filter>App Parts</Filter>
    </ClInclude>
    <ClInclude Include="App_Lifecycle.inl">
      <Filter>App Parts</Filter>
    </ClInclude>
    <ClInclude Include="App_UpdateInput.inl">
      <Filter>App Parts</Filter>
    </ClInclude>
    <ClInclude Include="App_RenderPasses.inl">
      <Filter>App Parts</Filter>
    </ClInclude>
    <ClInclude Include="App_ModelLoading.inl">
      <Filter>App Parts</Filter>
    </ClInclude>
    <ClInclude Include="App_ImGuiPanels.inl">
      <Filter>App Parts</Filter>
    </ClInclude>
    <ClInclude Include="App_Utilities.inl">
      <Filter>App Parts</Filter>
    </ClInclude>
```

- [ ] **Step 3: Validate XML and file registration**

Run:

```powershell
[xml](Get-Content -Raw -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj') | Out-Null
[xml](Get-Content -Raw -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj.filters') | Out-Null
rg -n "App_(InternalTypes|Lifecycle|UpdateInput|RenderPasses|ModelLoading|ImGuiPanels|Utilities)\\.inl" Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj.filters
```

Expected:

```text
XML parsing produces no errors.
Each App_*.inl appears in both vcxproj and filters.
```

- [ ] **Step 4: Commit the project registration**

Run:

```powershell
git add -- Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj.filters
git commit -m "chore: register advanced app implementation parts"
```

Expected:

```text
Commit succeeds with project metadata only.
```

---

### Task 4: Add Deferred Rendering README

**Files:**
- Create: `Dx11/35_DeferredRendering/README.md`

- [ ] **Step 1: Create the README content**

Create `Dx11/35_DeferredRendering/README.md` with:

```markdown
# 35. Deferred Rendering

이 예제는 Forward Rendering으로 그리던 장면을 G-Buffer 기반 Deferred Rendering 구조로 확장한 단계입니다. 목적은 "많은 조명을 한 번에 다루기 위해 렌더링을 지오메트리 패스와 라이팅 패스로 나눈다"는 핵심 흐름을 직접 확인하는 것입니다.

## 핵심 구현

- Geometry Pass: 모델의 위치, 노멀, 재질 정보를 여러 렌더 타겟에 기록합니다.
- Lighting Pass: 전체화면 Quad에서 G-Buffer를 읽어 조명을 계산합니다.
- Debug View: ImGui에서 G-Buffer를 확인해 어떤 값이 저장되는지 점검합니다.
- Tone Mapping: HDR 결과를 LDR/HDR 출력에 맞게 변환합니다.

## G-Buffer 구성

| 버퍼 | 내용 | 용도 |
|---|---|---|
| Position | 월드 공간 위치 | 조명 벡터, 감쇠 계산 |
| Normal | 월드 공간 노멀 | 난반사/정반사 계산 |
| Material | 금속성, 거칠기 등 | PBR 파라미터 |
| Albedo | 기본 색상 | 최종 조명 색상 |

## 렌더 흐름

```text
PassClear
 -> PassShadow
 -> PassGBuffer
 -> PassDeferredLight
 -> PassPostProcess
 -> PassUI
```

## 주요 파일

- `App.cpp`: 렌더 패스 구성과 G-Buffer 생성
- `35_DeferredGBufferVS.hlsl`, `35_DeferredGBufferPS.hlsl`: 지오메트리 패스
- `35_DeferredLightPS.hlsl`: 라이팅 패스
- `35_DeferredShared.fxh`: G-Buffer 공유 구조
- `35_ToneMappingPS_HDR.hlsl`, `35_ToneMappingPS_LDR.hlsl`: 톤매핑

## 포트폴리오에서 볼 포인트

- Forward와 Deferred의 패스 분리
- MRT(Multiple Render Targets) 사용
- 디버그 UI로 렌더 타겟 내용을 검증하는 흐름
- 36번 예제에서 애니메이션, 사운드, UI와 결합되기 전의 순수 Deferred Rendering 단계
```

- [ ] **Step 2: Verify README is present and linked path exists**

Run:

```powershell
Test-Path -LiteralPath 'Dx11/35_DeferredRendering/README.md'
rg -n "Deferred Rendering|G-Buffer|PassGBuffer|35_DeferredLightPS" Dx11/35_DeferredRendering/README.md
```

Expected:

```text
The file exists and contains Deferred Rendering, G-Buffer, PassGBuffer, and 35_DeferredLightPS.
```

- [ ] **Step 3: Commit the README**

Run:

```powershell
git add -- Dx11/35_DeferredRendering/README.md
git commit -m "docs: add deferred rendering overview"
```

Expected:

```text
Commit succeeds with one README.
```

---

### Task 5: Add Root README Representative Demo Section

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Insert the representative demo section before `### 프로젝트 바로가기`**

Add this block after the YouTube/Velog table:

```markdown
## 대표 데모

채용자가 빠르게 볼 대표 프로젝트는 [`36_AdvancedAnim_Sound_Click`](Dx11/36_AdvancedAnim_Sound_Click)입니다. 앞 단계에서 구현한 모델 로딩, PBR/IBL, 톤매핑, 디퍼드 렌더링 위에 애니메이션 블렌딩, 레이어, IK, 소켓, FMOD 3D 사운드, ImGui 디버그 UI, 멀티스레드 로딩을 묶은 최종 데모입니다.

| 항목 | 내용 |
|---|---|
| 실행 | `Dx11/TutorialApp.sln` 열기 -> `36_AdvancedAnim_Sound_Click` 시작 프로젝트 -> `x64` 빌드 |
| 렌더링 | Forward/Deferred 전환, Shadow, PBR, IBL, Tone Mapping |
| 애니메이션 | Blend, Additive, Layer, IK, Socket |
| 사운드/UI | FMOD 3D Sound, SoundBox, ImGui Debug Panels |
| 구조 | `Dx11/Common` 공통 코드 + `35_DeferredRendering` 렌더링 단계 + `36_AdvancedAnim_Sound_Click` 통합 데모 |

### 코드 구조 요약

```text
Dx11/Common/                         공통 D3D 앱, 카메라, 메시, 애니메이션, 사운드, 로더
Dx11/35_DeferredRendering/           G-Buffer, Deferred Lighting, Tone Mapping
Dx11/36_AdvancedAnim_Sound_Click/    대표 통합 데모
  App.cpp                            구현 파일 인덱스
  App_*.inl                          수명주기, 입력/업데이트, 렌더 패스, 모델 로딩, UI, 유틸리티
```
```

- [ ] **Step 2: Verify the section is readable and the old project table remains**

Run:

```powershell
rg -n "## 대표 데모|### 코드 구조 요약|### 프로젝트 바로가기|36_AdvancedAnim_Sound_Click" README.md
```

Expected:

```text
The new representative demo section appears before the existing project shortcut section.
```

- [ ] **Step 3: Commit the root README update**

Run:

```powershell
git add -- README.md
git commit -m "docs: highlight representative dx11 demo"
```

Expected:

```text
Commit succeeds with README.md only.
```

---

### Task 6: Final Verification

**Files:**
- Verify: `Dx11/36_AdvancedAnim_Sound_Click/App.cpp`
- Verify: `Dx11/36_AdvancedAnim_Sound_Click/App_*.inl`
- Verify: `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj`
- Verify: `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj.filters`
- Verify: `Dx11/35_DeferredRendering/README.md`
- Verify: `README.md`

- [ ] **Step 1: Run structural checks**

Run:

```powershell
$appLines = (Get-Content -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/App.cpp').Count
if ($appLines -ge 120) { throw "App.cpp is still too large: $appLines lines" }

$parts = @(
  'App_InternalTypes.inl',
  'App_Lifecycle.inl',
  'App_UpdateInput.inl',
  'App_RenderPasses.inl',
  'App_ModelLoading.inl',
  'App_ImGuiPanels.inl',
  'App_Utilities.inl'
)
foreach ($part in $parts) {
  $path = Join-Path 'Dx11/36_AdvancedAnim_Sound_Click' $part
  if (!(Test-Path -LiteralPath $path)) { throw "Missing $part" }
}

rg -n "#include \"App_.*\\.inl\"" Dx11/36_AdvancedAnim_Sound_Click/App.cpp
rg -n "LoadModelFromFile|RenderControlPannel|PassGBuffer|CreateSwapChainAndBackBuffer" Dx11/36_AdvancedAnim_Sound_Click/App_*.inl
```

Expected:

```text
App.cpp is under 120 lines.
All seven App_*.inl files exist.
Important functions are found in the split files.
```

- [ ] **Step 2: Run XML checks**

Run:

```powershell
[xml](Get-Content -Raw -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj') | Out-Null
[xml](Get-Content -Raw -LiteralPath 'Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj.filters') | Out-Null
```

Expected:

```text
No XML parsing errors.
```

- [ ] **Step 3: Run whitespace diff check**

Run:

```powershell
git diff --check
```

Expected:

```text
No whitespace errors.
```

- [ ] **Step 4: Try Visual Studio build if MSBuild is available**

Run:

```powershell
$msbuild = (Get-Command msbuild -ErrorAction SilentlyContinue)
if ($msbuild) {
  msbuild Dx11/TutorialApp.sln /p:Configuration=Debug /p:Platform=x64
} else {
  "MSBuild is not on PATH; skipped build verification."
}
```

Expected:

```text
Either MSBuild completes with exit code 0, or the output clearly says MSBuild is not on PATH.
```

- [ ] **Step 5: Commit any final verification-only metadata changes**

Run:

```powershell
git status --short
```

Expected:

```text
No uncommitted changes remain after the planned commits.
```

---

## Self-Review

- Spec coverage: the plan covers the App split, 35 README, root README, project registration, and verification requirements from `docs/superpowers/specs/2026-07-04-portfolio-code-structure-design.md`.
- Red-flag scan: no unresolved filler wording is intentionally left in this plan.
- Type consistency: all planned implementation part names match the spec and project registration steps.
