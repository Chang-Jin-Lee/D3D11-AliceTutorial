# Project README Visual Gallery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a consistent B-layout information PNG, freshly captured runtime PNG and GIF, and previous/main/up/next navigation to all 37 selected D3D11 project READMEs.

**Architecture:** Treat `tools/readme_media_manifest.json` as the single source of truth and place shared validation/path helpers in one dot-sourced PowerShell file. Keep capture, archive, poster generation, README generation, and verification as focused scripts that consume the same manifest; generated media stays under `docs/media/readme`, while replaced originals are copied to the external desktop archive before capture.

**Tech Stack:** PowerShell 7, Win32 window APIs, `System.Drawing`, Direct3D 11 Debug x64 executables, ffmpeg, Markdown, JSON.

## Global Constraints

- Cover exactly the 37 projects in `tools/readme_media_manifest.json`; do not add `Dx11/16_pmxWithMotion` to the sequence.
- Produce one 1600x900 PNG, one 800x450 8 fps GIF lasting four to five seconds, and one 1600x640 information PNG per project.
- Keep each GIF at or below 5 MiB; shorten to four seconds before reducing 800x450 resolution.
- Use only real runtime captures in the B-layout posters; do not generate fictional scene imagery.
- Preserve existing project README technical explanations and use relative GitHub-compatible links.
- Back up replaced media under `C:\Users\k2503200021\Desktop\애셋\D3D11-AliceTutorial\README_Media_<timestamp>` before overwriting it.
- Do not alter tutorial rendering behavior except for an environment-gated deterministic capture hook that exposes an already existing feature.
- Reject captures where the intended scene or character is absent, tiny, clipped, blank, or obscured.

## File Map

- Create `tools/readme_media_common.ps1`: manifest loading, schema validation, path resolution, and project selection shared by all tools.
- Modify `tools/readme_media_manifest.json`: metadata, three output paths, capture phase/actions, and fixed dimensions for all 37 projects.
- Create `tools/tests/test_readme_media_manifest.ps1`: dependency-free manifest contract tests.
- Create `tools/archive_readme_media.ps1`: non-destructive external archive with source commit metadata.
- Create `tools/tests/test_archive_readme_media.ps1`: temporary-directory archive test.
- Modify `tools/capture_readme_media.ps1`: exact client sizing, declarative actions, all-project GIF capture, two attempts, palette encoding, and richer report rows.
- Create `tools/tests/test_capture_manifest_actions.ps1`: action schema and selection tests that do not open GUI windows.
- Create `tools/generate_readme_info_images.ps1`: B-layout poster renderer.
- Create `tools/generate_readme_review_sheets.ps1`: PNG and GIF representative-frame contact sheets for manual QA.
- Create `tools/tests/test_readme_info_images.ps1`: poster and review-sheet dimensions/content test.
- Create `tools/update_project_readmes.ps1`: idempotent generated blocks for top navigation, information image, runtime media, and bottom navigation.
- Create `tools/tests/test_project_readme_updater.ps1`: two-project fixture test for links, preservation, and idempotence.
- Modify `tools/verify_readme_media.ps1`: dimensions, signatures, variance, GIF frame motion/size, README links, and report status checks.
- Create `tools/tests/test_verify_readme_media.ps1`: deliberately invalid fixture followed by valid fixture.
- Modify `README.md`: retain the 37-project PNG grid and document that every project page contains its GIF; keep project 36 as the root featured GIF.
- Modify the 37 manifest-selected `Dx11/<project>/README.md` files using the updater.
- Replace/create `docs/media/readme/*.png`, `docs/media/readme/*.gif`, `docs/media/readme/info/*.png`, and `docs/media/readme/capture-report.md` using the tools.

---

### Task 1: Establish The Manifest Contract

**Files:**
- Create: `tools/readme_media_common.ps1`
- Modify: `tools/readme_media_manifest.json`
- Create: `tools/tests/test_readme_media_manifest.ps1`

**Interfaces:**
- Produces: `Get-ReadmeMediaManifest([string]$ManifestPath, [string]$RepoRoot) -> PSCustomObject`
- Produces: `Test-ReadmeMediaManifest([object]$Manifest, [string]$RepoRoot) -> string[]` where an empty array means valid; project count is checked against the manifest's required `expectedProjectCount`.
- Produces: `Get-ReadmeMediaProject([object]$Manifest, [string]$Number) -> PSCustomObject`.
- Produces: `Resolve-ReadmeMediaPath([string]$RepoRoot, [string]$Path) -> string`.
- Consumed by: every later tool and test.

- [ ] **Step 1: Write the failing manifest contract test**

Create `tools/tests/test_readme_media_manifest.ps1` with a small assertion helper and these exact checks:

```powershell
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\readme_media_common.ps1')

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifest = Get-ReadmeMediaManifest -ManifestPath 'tools/readme_media_manifest.json' -RepoRoot $repoRoot
$errors = @(Test-ReadmeMediaManifest -Manifest $manifest -RepoRoot $repoRoot)

Assert-True ($errors.Count -eq 0) ($errors -join "`n")
Assert-True ($manifest.expectedProjectCount -eq 37) 'production manifest expectedProjectCount must be 37'
Assert-True (@($manifest.projects).Count -eq 37) 'manifest must contain 37 projects'
Assert-True (-not (@($manifest.projects.directory) -contains '16_pmxWithMotion')) 'duplicate project must stay excluded'
Assert-True ($manifest.captureWidth -eq 1600 -and $manifest.captureHeight -eq 900) 'PNG size contract mismatch'
Assert-True ($manifest.gifWidth -eq 800 -and $manifest.gifHeight -eq 450) 'GIF size contract mismatch'
Assert-True ($manifest.infoWidth -eq 1600 -and $manifest.infoHeight -eq 640) 'info image size contract mismatch'

foreach ($project in $manifest.projects) {
    Assert-True ($project.number -match '^\d{2}$') "invalid number: $($project.number)"
    Assert-True (-not [string]::IsNullOrWhiteSpace($project.title)) "missing title: $($project.number)"
    Assert-True (-not [string]::IsNullOrWhiteSpace($project.summary)) "missing summary: $($project.number)"
    Assert-True (@($project.tags).Count -ge 3 -and @($project.tags).Count -le 5) "invalid tags: $($project.number)"
    Assert-True ($project.image -match '\.png$') "missing PNG path: $($project.number)"
    Assert-True ($project.gif -match '\.gif$') "missing GIF path: $($project.number)"
    Assert-True ($project.infoImage -match '^info/.+-info\.png$') "missing info image path: $($project.number)"
}

'manifest contract tests passed'
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
```

Expected: FAIL because `tools/readme_media_common.ps1` does not exist and the current manifest lacks the new fields.

- [ ] **Step 3: Add shared manifest helpers**

Create `tools/readme_media_common.ps1`. Implement the four public functions from the Interfaces block. `Test-ReadmeMediaManifest` must return concrete errors for duplicate numbers/directories, project count different from `expectedProjectCount`, missing directories/READMEs, invalid dimensions, missing metadata, absent outputs, unsupported `gifPhase`, and malformed actions. Production sets `expectedProjectCount` to 37; one- and two-project test fixtures set it to their fixture count.

Use this action validation contract:

```powershell
$validGifPhases = @('startup', 'runtime')
$validActionTypes = @('wait', 'click', 'keyDown', 'keyUp')

switch ($action.type) {
    'wait' {
        if ([int]$action.durationMs -lt 0) { $errors.Add("negative wait duration: $number") }
    }
    'click' {
        if ([double]$action.x -lt 0 -or [double]$action.x -gt 1 -or
            [double]$action.y -lt 0 -or [double]$action.y -gt 1) {
            $errors.Add("click coordinates must be normalized: $number")
        }
    }
    'keyDown' { if ($action.key -notmatch '^[WASD]$') { $errors.Add("unsupported key: $number") } }
    'keyUp'   { if ($action.key -notmatch '^[WASD]$') { $errors.Add("unsupported key: $number") } }
}
```

Path resolution must accept either a repository-relative or absolute manifest path while always returning full paths for generated assets.

- [ ] **Step 4: Expand all 37 manifest entries**

Add top-level values:

```json
"expectedProjectCount": 37,
"captureWidth": 1600,
"captureHeight": 900,
"gifWidth": 800,
"gifHeight": 450,
"gifSeconds": 4,
"gifFps": 8,
"gifMaxBytes": 5242880,
"infoWidth": 1600,
"infoHeight": 640,
"captureAttempts": 2
```

For every project add `title`, `summary`, `tags`, `gif`, `infoImage`, and `gifPhase`. Derive `gif` from the existing image stem with a lowercase `.gif` extension and derive `infoImage` as `info/<image-stem>-info.png`. Keep project 36's existing lowercase GIF filename to avoid breaking the root featured link, and add `rootFeaturedGif: true` only to project 36.

Use these exact content themes for the metadata so the poster copy stays concise and technically accurate:

| No. | Title | Summary theme | Tags |
|---|---|---|---|
| 01 | Rendering Quadangle | 두 개의 삼각형으로 NDC 사각형을 렌더링합니다. | D3D11, Vertex Buffer, Index Buffer |
| 02 | Rendering Cube | 월드·뷰·투영 행렬로 3D 큐브를 렌더링합니다. | Transform, Camera, Depth |
| 03 | Mesh And Scene Graph | 메시와 부모·자식 트랜스폼으로 장면을 구성합니다. | Mesh, Scene Graph, Transform |
| 04 | Mesh With Texture | UV와 셰이더 리소스로 텍스처 메시를 렌더링합니다. | Texture, UV, Sampler |
| 05 | Mesh FBX | Assimp로 FBX 메시를 읽어 D3D11에서 렌더링합니다. | FBX, Assimp, Mesh |
| 06 | PMX A-Pose | PMX 캐릭터의 기본 포즈와 메시 구조를 표시합니다. | PMX, MMD, Skeletal Mesh |
| 07 | PMX Texture | PMX 재질과 텍스처를 함께 로드합니다. | PMX, Texture, Material |
| 08 | ImGui System Info | ImGui에서 FPS와 시스템 정보를 실시간 표시합니다. | ImGui, Profiling, System Info |
| 09 | Lighting | 법선과 광원 벡터를 사용해 기본 조명을 계산합니다. | Lighting, Normal, Shader |
| 10 | Static Cube SkyBox | 정적 큐브와 스카이박스로 3D 환경을 구성합니다. | Skybox, Cubemap, Camera |
| 11 | Live2D | Cubism 모델을 D3D11 렌더링 루프에 통합합니다. | Live2D, Cubism, UI |
| 12 | Blinn-Phong Lighting | 하프 벡터를 사용하는 Blinn-Phong 반사광을 구현합니다. | Blinn-Phong, Specular, Lighting |
| 13 | Line Renderer Axis Debug | 라인 렌더러로 월드 축과 디버그 기즈모를 표시합니다. | Line Renderer, Axis, Debug Draw |
| 14 | Phong Lighting | 반사 벡터 기반 Phong 조명 모델을 구현합니다. | Phong, Specular, Shader |
| 15 | PMX With Phong | PMX 캐릭터 재질에 Phong 조명을 적용합니다. | PMX, Phong, Material |
| 16 | Normal Mapping | TBN 공간의 노멀 맵으로 표면 디테일을 표현합니다. | Normal Map, TBN, Texture |
| 17 | FBX PMX OBJ With Phong | FBX·PMX·OBJ를 공통 메시 경로로 로드합니다. | Assimp, Multi Format, Phong |
| 18 | FBX Animation | 본 계층과 스키닝 행렬로 FBX 애니메이션을 재생합니다. | Animation, Skinning, Bone |
| 19 | Multi Models | 여러 모델 인스턴스를 한 장면에서 관리합니다. | Multi Model, Scene, Resource |
| 20 | Depth And Alpha | 깊이 테스트와 알파 블렌딩 순서를 비교합니다. | Depth, Alpha, Blend |
| 21 | Multi Models With Animations | 여러 스켈레탈 모델의 애니메이션을 동시에 갱신합니다. | Animation, Multi Model, Skinning |
| 22 | VMD Camera | VMD 카메라 모션을 장면 카메라에 적용합니다. | VMD, Camera, Motion |
| 23 | Rigid And Skinned Animation | 강체 변환과 스키닝 애니메이션을 함께 비교합니다. | Rigid, Skinning, Animation |
| 24 | Skinned Bone Structure | 스키닝 본 계층과 버텍스 영향을 시각화합니다. | Skeleton, Bone, Skinning |
| 25 | Toon Shading Outline | 툰 명암과 외곽선 패스를 조합합니다. | Toon, Outline, Multi Pass |
| 26 | Shadow Map PCF | 섀도 맵과 PCF 필터로 부드러운 그림자를 만듭니다. | Shadow Map, PCF, Depth |
| 27 | Debug Draw Box | 바운딩 박스와 디버그 선을 장면 위에 렌더링합니다. | Debug Draw, Bounding Box, Line |
| 28 | Shared Model Animation | 공유 모델 리소스로 여러 애니메이션 인스턴스를 렌더링합니다. | Shared Resource, Animation, Instance |
| 29 | Mouse Picking | 화면 좌표의 레이로 3D 오브젝트를 선택합니다. | Picking, Ray Cast, Input |
| 30 | PBR BRDF | 금속성·거칠기 기반 BRDF 재질을 구현합니다. | PBR, BRDF, Material |
| 31 | Image Based Lighting | 환경 맵을 사용해 PBR 간접광을 계산합니다. | IBL, HDR, PBR |
| 32 | FMOD Sound | FMOD 사운드 재생과 디버그 제어를 통합합니다. | FMOD, Audio, ImGui |
| 33 | Sound Animation Camera Motion | 사운드·캐릭터 애니메이션·카메라 모션을 동기화합니다. | Audio, Animation, Camera |
| 34 | Tone Mapping | HDR 장면을 화면 출력 범위로 톤매핑합니다. | HDR, Tone Mapping, Post Process |
| 35 | Deferred Rendering | G-Buffer와 디퍼드 라이팅 패스를 구현합니다. | Deferred, G-Buffer, Lighting |
| 36 | Advanced Animation And Sound | 블렌드·레이어·IK·소켓과 3D 사운드를 통합합니다. | Animation, IK, FMOD, ImGui |
| 37 | Blueprint Node Editor | imgui-node-editor로 노드 기반 도구 UI를 구성합니다. | Node Editor, ImGui, Tooling |

Use `gifPhase: "startup"` for projects 01 and 37. Use passive runtime capture for 02, 08, 11, 18, 21-24, 28, 33, and 36 because they already contain visible motion or changing UI. For the remaining static 3D projects, add this exact gentle camera sway to `gifActions`:

```json
[
  {"atMs": 400, "type": "keyDown", "key": "D"},
  {"atMs": 1100, "type": "keyUp", "key": "D"},
  {"atMs": 2200, "type": "keyDown", "key": "A"},
  {"atMs": 2900, "type": "keyUp", "key": "A"}
]
```

Convert project 36's `startClick` into `preCaptureActions: [{"type":"click","x":0.5,"y":0.5},{"type":"wait","durationMs":1500}]`.

- [ ] **Step 5: Run the contract test and verify it passes**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
```

Expected: `manifest contract tests passed`.

- [ ] **Step 6: Commit the manifest contract**

```powershell
git add tools/readme_media_common.ps1 tools/readme_media_manifest.json tools/tests/test_readme_media_manifest.ps1
git commit -m "test: define README media manifest contract"
```

---

### Task 2: Archive Existing README Media Non-Destructively

**Files:**
- Create: `tools/archive_readme_media.ps1`
- Create: `tools/tests/test_archive_readme_media.ps1`

**Interfaces:**
- Consumes: `Get-ReadmeMediaManifest` and repository-relative paths from Task 1.
- Produces: an archive directory containing `docs/media/readme`, root READMEs, selected project READMEs, `archive-manifest.json`, and no modifications to source files.

- [ ] **Step 1: Write the failing archive test**

Create a temporary repository fixture with `README.md`, `README_old.md`, one project README, one PNG, and a one-project fixture manifest. Invoke the archive script with an explicit temporary destination and assert copied hashes match source hashes:

```powershell
$before = (Get-FileHash $sourcePng -Algorithm SHA256).Hash
& $archiveScript -RepoRoot $fixtureRoot -Manifest $fixtureManifest -DestinationRoot $destination
$archive = Get-ChildItem $destination -Directory | Select-Object -First 1
if ($null -eq $archive) { throw 'archive directory was not created' }
$copy = Join-Path $archive.FullName 'docs\media\readme\01.png'
if ((Get-FileHash $copy -Algorithm SHA256).Hash -ne $before) { throw 'archived PNG hash mismatch' }
if ((Get-FileHash $sourcePng -Algorithm SHA256).Hash -ne $before) { throw 'source PNG was modified' }
if (-not (Test-Path (Join-Path $archive.FullName 'archive-manifest.json'))) { throw 'archive metadata missing' }
'archive tests passed'
```

- [ ] **Step 2: Run the test and verify it fails**

Run `pwsh -NoProfile -File tools/tests/test_archive_readme_media.ps1`.

Expected: FAIL because `tools/archive_readme_media.ps1` does not exist.

- [ ] **Step 3: Implement the archive tool**

Parameters:

```powershell
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Manifest = 'tools/readme_media_manifest.json',
    [string]$DestinationRoot = 'C:\Users\k2503200021\Desktop\애셋\D3D11-AliceTutorial'
)
```

Resolve and validate the manifest, create `README_Media_yyyyMMdd_HHmmss`, copy the entire existing `docs/media/readme` directory, copy root `README.md`/`README_old.md`, and copy the 37 selected project READMEs while preserving `Dx11/<directory>/README.md`. Write `archive-manifest.json` with `createdAt`, `sourceRoot`, `sourceCommit` from `git rev-parse HEAD`, and sorted relative file paths plus SHA-256 hashes. Never move or delete a source path.

- [ ] **Step 4: Run the archive test and verify it passes**

Run `pwsh -NoProfile -File tools/tests/test_archive_readme_media.ps1`.

Expected: `archive tests passed`.

- [ ] **Step 5: Commit the archive tool**

```powershell
git add tools/archive_readme_media.ps1 tools/tests/test_archive_readme_media.ps1
git commit -m "feat: archive README media before capture"
```

---

### Task 3: Extend The Capture Engine

**Files:**
- Modify: `tools/capture_readme_media.ps1`
- Create: `tools/tests/test_capture_manifest_actions.ps1`

**Interfaces:**
- Consumes: manifest dimensions, `preCaptureActions`, `gifActions`, `gifPhase`, and `captureAttempts`.
- Produces: exact-size PNG/GIF outputs plus capture-report rows containing attempt number, dimensions, byte count, and status.
- Produces: optional `-OutputDir` override for smoke tests; production defaults to the manifest `mediaDir`.

- [ ] **Step 1: Write failing non-GUI tests for action scheduling and project selection**

Add a `-ValidateOnly` switch to the intended CLI contract, then create `tools/tests/test_capture_manifest_actions.ps1` to invoke:

```powershell
$validation = & $captureScript -Manifest 'tools/readme_media_manifest.json' -ValidateOnly
if ($validation -notcontains 'capture manifest validation passed') { throw 'capture manifest validation failed' }

$project36 = Get-ReadmeMediaProject -Manifest $manifest -Number '36'
if (@($project36.preCaptureActions).Count -ne 2) { throw 'project 36 start actions missing' }
$sway = @($manifest.projects | Where-Object { @($_.gifActions).Count -eq 4 })
if ($sway.Count -lt 1) { throw 'camera sway actions missing' }
'capture action tests passed'
```

- [ ] **Step 2: Run the tests and verify they fail**

Run `pwsh -NoProfile -File tools/tests/test_capture_manifest_actions.ps1`.

Expected: FAIL because `-ValidateOnly` and declarative action execution are not implemented.

- [ ] **Step 3: Dot-source common helpers and add exact client sizing**

Replace the script-local JSON/path helpers with `tools/readme_media_common.ps1`. Extend the Win32 type with `WM_KEYDOWN`, `WM_KEYUP`, and client/outer rectangle sizing. Add:

```powershell
function Resize-CaptureWindowClient {
    param([IntPtr]$Handle, [int]$ClientWidth, [int]$ClientHeight)
    $window = New-Object ReadmeCaptureWin32+RECT
    $client = New-Object ReadmeCaptureWin32+RECT
    if (-not [ReadmeCaptureWin32]::GetWindowRect($Handle, [ref]$window) -or
        -not [ReadmeCaptureWin32]::GetClientRect($Handle, [ref]$client)) {
        throw 'unable to measure capture window'
    }
    $outerWidth = $ClientWidth + (($window.Right - $window.Left) - ($client.Right - $client.Left))
    $outerHeight = $ClientHeight + (($window.Bottom - $window.Top) - ($client.Bottom - $client.Top))
    if (-not [ReadmeCaptureWin32]::SetWindowPos($Handle, [IntPtr]::Zero, 40, 40, $outerWidth, $outerHeight, 0x0040)) {
        throw 'unable to resize capture window'
    }
}
```

After resizing, re-read the client rectangle and fail unless it is exactly 1600x900.

- [ ] **Step 4: Implement declarative pre-capture and timed GIF actions**

Map W/A/S/D to virtual key codes `0x57/0x41/0x53/0x44`. `click` uses normalized client coordinates; `wait` sleeps only in pre-capture actions. During GIF frame capture, dispatch every action whose `atMs` is less than or equal to elapsed capture time before saving that frame. In `finally`, post key-up for every key that was pressed so a failed capture cannot leave input stuck.

For `gifPhase: startup`, resize the first available client window and record the GIF before the project load delay; then wait and capture the final PNG. For `gifPhase: runtime`, run pre-capture actions, capture the PNG, and then record the GIF.

- [ ] **Step 5: Encode optimized GIFs and enforce the size contract**

Replace the current direct GIF filter with ffmpeg's split palette pipeline:

```powershell
$filter = "fps=$gifFps,scale=$gifWidth`:$gifHeight`:flags=lanczos,split[s0][s1];" +
          '[s0]palettegen=max_colors=128:stats_mode=diff[p];' +
          '[s1][p]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle'
```

Encode at 8 fps and 800x450. If output exceeds 5,242,880 bytes, re-encode with `max_colors=96`; if it still exceeds the limit, fail the project rather than silently committing an oversized file.

- [ ] **Step 6: Add two-attempt project isolation and report details**

Wrap one project's full launch/capture/close lifecycle in `Invoke-ProjectCapture`. Call it up to `captureAttempts` times, always closing the prior process before retrying. A successful report row includes `Attempt`, `Dimensions`, and `Bytes`; failed attempts retain the exception text. The script exits nonzero if any project's final attempt fails.

- [ ] **Step 7: Run tests and one-project integration capture**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_capture_manifest_actions.ps1
$smokeDir = Join-Path $env:TEMP 'D3D11-readme-capture-smoke'
pwsh -NoProfile -File tools/capture_readme_media.ps1 -ProjectNumber 36 -OutputDir $smokeDir
```

Expected: tests print `capture action tests passed`; project 36 produces a 1600x900 PNG and 800x450 GIF with successful report rows and no orphan `frames-*` directory.

- [ ] **Step 8: Commit the capture engine**

```powershell
git add tools/capture_readme_media.ps1 tools/tests/test_capture_manifest_actions.ps1
git commit -m "feat: capture consistent README PNG and GIF media"
```

---

### Task 4: Generate B-Layout Information Images And Review Sheets

**Files:**
- Create: `tools/generate_readme_info_images.ps1`
- Create: `tools/generate_readme_review_sheets.ps1`
- Create: `tools/tests/test_readme_info_images.ps1`

**Interfaces:**
- Consumes: manifest metadata and freshly captured PNG/GIF paths.
- Produces: 1600x640 `infoImage` files and optional PNG/GIF contact sheets in a caller-selected review directory.

- [ ] **Step 1: Write the failing image-generation test**

The test creates a 1600x900 four-color fixture PNG, a one-project fixture manifest, and runs both scripts. Assert the poster is 1600x640, the left screenshot region has multiple colors, the right summary region is not a single color, and both review sheets are created.

```powershell
$poster = [System.Drawing.Image]::FromFile($posterPath)
try {
    if ($poster.Width -ne 1600 -or $poster.Height -ne 640) { throw 'poster dimensions mismatch' }
}
finally { $poster.Dispose() }

if (-not (Test-Path $pngSheet) -or -not (Test-Path $gifSheet)) { throw 'review sheets missing' }
'info image tests passed'
```

- [ ] **Step 2: Run the test and verify it fails**

Run `pwsh -NoProfile -File tools/tests/test_readme_info_images.ps1`.

Expected: FAIL because the two generator scripts do not exist.

- [ ] **Step 3: Implement the B-layout poster renderer**

Use `System.Drawing` with high-quality bicubic interpolation and antialiasing. Draw a `1600x640` graphite background, place the screenshot aspect-fit inside `(40,40,940,560)`, and draw project number, wrapped title, wrapped Korean summary, and three-to-five tag labels inside `(1030,64,520,512)`. Use `Malgun Gothic` when available and fall back to `Segoe UI`; reduce title font from 46 down to 32 until it fits. Dispose every bitmap, graphics, font, brush, pen, and string format in `finally`.

The script parameters are:

```powershell
param(
    [string]$Manifest = 'tools/readme_media_manifest.json',
    [string]$ProjectNumber,
    [switch]$All
)
```

Require exactly one of `-All` and `-ProjectNumber`, matching the capture CLI.

- [ ] **Step 4: Implement review contact sheets**

Generate a five-column contact sheet for the 37 PNGs and a second sheet using each GIF's midpoint frame. Each cell contains the two-digit number above an aspect-fit thumbnail. Default output directory is `$env:TEMP\D3D11-AliceTutorial-readme-review`; `-OutputDir` overrides it. These sheets are review artifacts and are not added to Git.

- [ ] **Step 5: Run image tests and verify they pass**

Run `pwsh -NoProfile -File tools/tests/test_readme_info_images.ps1`.

Expected: `info image tests passed`.

- [ ] **Step 6: Commit the generators**

```powershell
git add tools/generate_readme_info_images.ps1 tools/generate_readme_review_sheets.ps1 tools/tests/test_readme_info_images.ps1
git commit -m "feat: generate README information posters"
```

---

### Task 5: Generate Consistent Project README Navigation And Media Blocks

**Files:**
- Create: `tools/update_project_readmes.ps1`
- Create: `tools/tests/test_project_readme_updater.ps1`
- Modify later through generator: the 37 selected `Dx11/<directory>/README.md` files

**Interfaces:**
- Consumes: ordered manifest entries and their `directory`, `image`, `gif`, and `infoImage` values.
- Produces: idempotent `README-NAV-TOP`, `README-INFO`, `README-RUNTIME`, and `README-NAV-BOTTOM` generated blocks while preserving text outside the markers byte-for-byte except normalized final newline.

- [ ] **Step 1: Write the failing two-project fixture test**

Create two fixture READMEs containing distinct existing Korean body text. Run the updater twice and assert:

```powershell
& $updater -RepoRoot $fixtureRoot -Manifest $fixtureManifest
$first = Get-Content -Raw $readme01
& $updater -RepoRoot $fixtureRoot -Manifest $fixtureManifest
$second = Get-Content -Raw $readme01

if ($first -cne $second) { throw 'README update is not idempotent' }
if ($first -notmatch '기존 기술 설명') { throw 'existing body was not preserved' }
if ($first -notmatch '\.\./\.\./README\.md') { throw 'main link missing' }
if ($first -notmatch '\.\./02_Test/README\.md') { throw 'next link missing' }
if ($first -notmatch 'docs/media/readme/info/01-Test-info\.png') { throw 'info image missing' }
if ($first -notmatch 'docs/media/readme/01-Test\.gif') { throw 'GIF missing' }
'project README updater tests passed'
```

- [ ] **Step 2: Run the test and verify it fails**

Run `pwsh -NoProfile -File tools/tests/test_project_readme_updater.ps1`.

Expected: FAIL because the updater does not exist.

- [ ] **Step 3: Implement generated top and bottom blocks**

Use these exact marker names and ordering:

```markdown
<!-- README-NAV-TOP:START -->
<div align="center">

이전 | [메인](../../README.md) | [상위](../) | [다음](../02_RenderingCube/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/01-RenderingQuadangle-info.png" width="100%" /></p>
<!-- README-INFO:END -->
```

The first project's Previous and last project's Next are non-linked text. For middle projects use `../<directory>/README.md`. Append this runtime block after the preserved body:

```markdown
<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/01-RenderingQuadangle.png" width="100%" /> | <img src="../../docs/media/readme/01-RenderingQuadangle.gif" width="100%" /> |
<!-- README-RUNTIME:END -->
```

Then append `README-NAV-BOTTOM` with the same links. On subsequent runs replace only text between matching generated markers. Never use a regular expression that removes unmarked tables or images.

- [ ] **Step 4: Run the fixture test and verify it passes**

Run `pwsh -NoProfile -File tools/tests/test_project_readme_updater.ps1`.

Expected: `project README updater tests passed`.

- [ ] **Step 5: Run the updater against all 37 project READMEs**

Run:

```powershell
pwsh -NoProfile -File tools/update_project_readmes.ps1 -All
```

Inspect projects 01, 17, 36, and 37. Confirm project 17's existing format-specific explanation remains present, project 36 retains its retargeting explanation, and first/last disabled links are correct.

- [ ] **Step 6: Commit the updater and README structure**

```powershell
git add tools/update_project_readmes.ps1 tools/tests/test_project_readme_updater.ps1 Dx11/*/README.md
git commit -m "docs: add project README gallery navigation"
```

---

### Task 6: Strengthen Media Verification

**Files:**
- Modify: `tools/verify_readme_media.ps1`
- Create: `tools/tests/test_verify_readme_media.ps1`
- Modify: `README.md`

**Interfaces:**
- Consumes: manifest and generated files from Tasks 1-5.
- Produces: exit code 0 only when all 37 media triplets, generated README blocks, root PNG links, project 36 root featured GIF, and successful capture report are valid.

- [ ] **Step 1: Write the failing verifier fixture test**

Create a temporary one-project fixture with wrong-sized/solid PNGs, an oversized or one-frame GIF, missing README blocks, and a failure capture report. Invoke the verifier with `-RepoRoot` and `-Manifest`; assert it exits nonzero and mentions dimensions, variance/motion, README, and report status. Replace the fixture with valid multi-color PNGs, a two-frame moving GIF, correct markers/links, and a Success report; assert exit code 0.

- [ ] **Step 2: Run the test and verify it fails**

Run `pwsh -NoProfile -File tools/tests/test_verify_readme_media.ps1`.

Expected: FAIL because the current verifier has no fixture parameters or deep validation.

- [ ] **Step 3: Implement PNG, poster, and GIF validation**

Add `-RepoRoot` and `-Manifest` parameters. Use `System.Drawing.Image` for dimensions and GIF frame enumeration. Sample a regular pixel grid:

- PNG/info images: require at least eight unique sampled RGB values and luminance variance above 4.0.
- GIF: require at least two frames, 800x450 dimensions, total decoded delay between 3.5 and 5.5 seconds, sampled RGB change above 0.2% between the first and the most-different sampled frame, and file length at most 5,242,880 bytes.
- Validate PNG signature and GIF87a/GIF89a signature before decoding.

Collect every error and print all of them before exiting 1 so one run identifies every bad project.

- [ ] **Step 4: Validate README and report contracts**

For every selected project require exactly one start/end pair for each generated marker, correct top/bottom navigation, correct three media paths, and preserved non-generated body content of at least 50 characters. Require all root PNG links; require root GIF links only for manifest entries with `rootFeaturedGif: true`. Parse `capture-report.md` and fail if any final project status is not Success.

- [ ] **Step 5: Update the root README copy**

Keep the existing 37-project screenshot grid and project 36 featured GIF. Add one concise sentence under `### 프로젝트 바로가기`: `각 프로젝트 README에서 새 실행 스크린샷과 짧은 GIF를 함께 확인할 수 있습니다.` Do not embed 37 GIFs in the root README.

- [ ] **Step 6: Run verifier tests and verify they pass**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_verify_readme_media.ps1
pwsh -NoProfile -File tools/verify_readme_media.ps1
```

Expected: fixture test passes; repository verification may still fail because the full 37-project capture and posters have not yet been generated.

- [ ] **Step 7: Commit the verifier**

```powershell
git add tools/verify_readme_media.ps1 tools/tests/test_verify_readme_media.ps1 README.md
git commit -m "test: verify README gallery media"
```

---

### Task 7: Capture, Inspect, Recapture, And Commit All 37 Projects

**Files:**
- Replace: `docs/media/readme/<37 image files>.png`
- Create: `docs/media/readme/<37 project files>.gif`
- Create: `docs/media/readme/info/<37 project files>-info.png`
- Modify: `docs/media/readme/capture-report.md`
- Verify: all 37 selected `Dx11/<directory>/README.md` files and `README.md`

**Interfaces:**
- Consumes: all tools and manifest contracts from Tasks 1-6.
- Produces: reviewed final documentation media and a clean, verifiable Git state.

- [ ] **Step 1: Run every dependency-free tool test**

```powershell
$tests = Get-ChildItem tools/tests/test_readme_*.ps1,tools/tests/test_archive_*.ps1,tools/tests/test_capture_*.ps1 -ErrorAction Stop
foreach ($test in $tests) { pwsh -NoProfile -File $test.FullName; if ($LASTEXITCODE -ne 0) { throw "failed: $($test.Name)" } }
```

Expected: every script prints its `... tests passed` message.

- [ ] **Step 2: Create the external original-media archive**

```powershell
pwsh -NoProfile -File tools/archive_readme_media.ps1
```

Expected: one new timestamped directory under `C:\Users\k2503200021\Desktop\애셋\D3D11-AliceTutorial`, containing media, root/project READMEs, and `archive-manifest.json`. Verify a sample of five archived hashes against their source hashes before capture.

- [ ] **Step 3: Build Debug x64**

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
& $msbuild 'Dx11\TutorialApp.sln' /m /p:Configuration=Debug /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw 'Debug x64 build failed' }
```

Expected: build completes with zero errors and all 37 manifest executables exist under `Dx11/bin`.

- [ ] **Step 4: Capture all runtime PNGs and GIFs**

```powershell
pwsh -NoProfile -File tools/capture_readme_media.ps1 -All
```

Expected: 37 successful PNG rows and 37 successful GIF rows; no final Failure rows; each launched process is closed.

- [ ] **Step 5: Generate all information images and review sheets**

```powershell
pwsh -NoProfile -File tools/generate_readme_info_images.ps1 -All
$reviewDir = Join-Path $env:TEMP 'D3D11-AliceTutorial-readme-review'
pwsh -NoProfile -File tools/generate_readme_review_sheets.ps1 -OutputDir $reviewDir
```

Expected: 37 posters and two contact sheets.

- [ ] **Step 6: Perform the mandatory visual review and targeted recaptures**

Open both contact sheets with `view_image`. Reject any project whose intended subject is missing, very small, clipped, covered by unrelated windows, or blank. Inspect the first/mid/last frames of every GIF, with focused checks for 17-24, 26-36, and project 36 retargeted animation. Enter the rejected project numbers when prompted; pressing Enter skips the loop when none were rejected:

```powershell
$answer = Read-Host 'Rejected project numbers, comma-separated'
$failedProjects = @($answer.Split(',', [System.StringSplitOptions]::RemoveEmptyEntries) | ForEach-Object { $_.Trim() })
foreach ($number in $failedProjects) {
    pwsh -NoProfile -File tools/capture_readme_media.ps1 -ProjectNumber $number
    if ($LASTEXITCODE -ne 0) { throw "recapture failed: $number" }
    pwsh -NoProfile -File tools/generate_readme_info_images.ps1 -ProjectNumber $number
    if ($LASTEXITCODE -ne 0) { throw "poster regeneration failed: $number" }
}
```

Repeat review-sheet generation after the last recapture.

- [ ] **Step 7: Regenerate READMEs and run complete verification**

```powershell
pwsh -NoProfile -File tools/update_project_readmes.ps1 -All
pwsh -NoProfile -File tools/verify_readme_media.ps1
git diff --check
```

Expected: `README media verification passed`; `git diff --check` prints nothing.

- [ ] **Step 8: Inspect the final diff and repository size impact**

```powershell
git status --short
git diff --stat
$gifBytes = (Get-ChildItem docs/media/readme -Filter *.gif | Measure-Object Length -Sum).Sum
"GIF total MiB: {0:N2}" -f ($gifBytes / 1MB)
```

Confirm exactly 37 GIFs, 37 information PNGs, 37 runtime PNGs, all 37 project README changes, and the expected tool changes are present. Confirm no `frames-*`, temporary fixture, review-sheet, executable, or desktop archive path is staged.

- [ ] **Step 9: Commit the reviewed media and final README output**

```powershell
git add README.md Dx11/*/README.md docs/media/readme tools/readme_media_manifest.json
git commit -m "docs: add visual gallery for all D3D11 projects"
```

- [ ] **Step 10: Run final post-commit verification**

```powershell
pwsh -NoProfile -File tools/verify_readme_media.ps1
git diff --check HEAD^ HEAD
git status --short
```

Expected: verification passes, diff check is silent, and the working tree is clean.

---

## Final Review Checklist

- [ ] All 37 selected project READMEs show the approved B-layout poster first.
- [ ] Previous/main/up/next links work at both the top and bottom, including first/last disabled states.
- [ ] Every project has a fresh real 1600x900 PNG and an 800x450 motion GIF at or below 5 MiB.
- [ ] Every information image is 1600x640 and uses its matching real screenshot.
- [ ] Existing technical prose remains present, especially in projects 17, 18, 36, and 37.
- [ ] Project 36 animation is visually intact in representative GIF frames.
- [ ] Replaced originals are preserved in the timestamped desktop archive.
- [ ] Build, tool tests, media verification, contact-sheet review, and Git cleanliness all pass.
