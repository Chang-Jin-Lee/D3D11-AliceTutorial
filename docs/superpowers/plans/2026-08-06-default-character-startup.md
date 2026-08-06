# Default Character Startup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make projects 17, 18, 19, 20, 21, 23, and 24 load the current `SampleModel.glb` at world position `(0, 0, 0)` during normal startup while preserving README-capture-only presentation settings.

**Architecture:** Keep each tutorial's existing model loader and state representation. Move the default-model load success block outside `ReadmeCapture::IsEnabled()`, keep the capture scale/rotation/camera overrides in a nested capture-only block, and enforce the seven-project contract with one PowerShell source regression test.

**Tech Stack:** C++/Direct3D 11, Assimp-backed existing model loaders, PowerShell 5.1+, Visual Studio 2022 Build Tools/MSBuild, existing README capture tooling.

## Global Constraints

- Work in an isolated worktree created with `superpowers:using-git-worktrees`; the user's main checkout contains intentional uncommitted model files.
- Modify exactly projects 17, 18, 19, 20, 21, 23, and 24; do not change project 22 or any other tutorial.
- Load `..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb`, the current swimsuit model; do not switch to `SampleModel2.glb`.
- Keep the character's normal-startup world position at exactly `(0, 0, 0)`.
- Keep scale `80`, rotation `(0, -35, 0)`, automatic rotation, and README camera overrides inside `ReadmeCapture::IsEnabled()`.
- A default-model load failure must skip model-specific indexing and presentation overrides but must not fail the whole app initialization.
- Do not modify, stage, or commit `Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb`, `Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel2.glb`, or `.superpowers/`.
- Preserve each `App.cpp` file's existing formatting and encoding by applying narrow ASCII-context patches only.
- Do not add a shared helper or refactor unrelated loader, renderer, animation, or UI code.

## File Structure

- `tools/tests/test_default_character_startup.ps1` — central static contract for the seven startup paths, origin guarantees, capture-only presentation, and safe list access.
- `Dx11/17_fbx_pmx_obj_WithPhong/App.cpp` — always load the single default model; retain its loader-provided origin.
- `Dx11/18_fbx_Animation/App.cpp` — always load the animated single default model; retain its loader-provided origin.
- `Dx11/19_MultiModels/App.cpp` — always append the default model and explicitly set the new entry to the origin.
- `Dx11/20_Depth_And_Alpha_Issue/App.cpp` — always append the default model and explicitly set the new entry to the origin.
- `Dx11/21_MultiModels_With_Animations/App.cpp` — always append the default animated model and explicitly set the new entry to the origin.
- `Dx11/23_Rigid_Animation/App.cpp` — always append the default rigid model and explicitly set the new entry to the origin.
- `Dx11/24_Skinned_With_Bone_Structure/App.cpp` — always append the default skinned model and explicitly set the new entry to the origin.

---

### Task 1: Add the startup contract and fix all seven initialization paths

**Files:**

- Create: `tools/tests/test_default_character_startup.ps1`
- Modify: `Dx11/17_fbx_pmx_obj_WithPhong/App.cpp:290-300`
- Modify: `Dx11/18_fbx_Animation/App.cpp:333-343`
- Modify: `Dx11/19_MultiModels/App.cpp:314-326`
- Modify: `Dx11/20_Depth_And_Alpha_Issue/App.cpp:311-323`
- Modify: `Dx11/21_MultiModels_With_Animations/App.cpp:314-326`
- Modify: `Dx11/23_Rigid_Animation/App.cpp:314-326`
- Modify: `Dx11/24_Skinned_With_Bone_Structure/App.cpp:495-507`
- Test: `tools/tests/test_default_character_startup.ps1`
- Test: `tools/tests/test_visual_capture_contracts.ps1`

**Interfaces:**

- Consumes: each project's existing `bool App::LoadModelFromFile(const std::wstring& pathW)`, `ReadmeCapture::IsEnabled()`, camera setters, and either single-model state (`m_modelPos`) or the list-model `ModelEntry` fields (`pos`, `scale`, `rotDeg`, `autoRotate`).
- Produces: an `App::OnInitialize()` contract in which default loading runs in normal and capture modes, while presentation overrides run only in capture mode; a reusable PowerShell acceptance test that exits nonzero on any regression.

- [ ] **Step 1: Write the failing startup contract test**

Create `tools/tests/test_default_character_startup.ps1` with this complete content:

```powershell
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-BracedBlock([string]$Text, [int]$Anchor, [string]$Label) {
    $openBrace = $Text.IndexOf('{', $Anchor)
    Assert-True ($openBrace -ge 0) "$Label opening brace missing"

    $depth = 0
    for ($index = $openBrace; $index -lt $Text.Length; ++$index) {
        if ($Text[$index] -eq '{') { ++$depth }
        elseif ($Text[$index] -eq '}') {
            --$depth
            if ($depth -eq 0) {
                return $Text.Substring($Anchor, $index - $Anchor + 1)
            }
        }
    }

    throw "$Label closing brace missing"
}

function Assert-CaptureOnlySetting(
    [string]$InitializeBody,
    [string]$CaptureBlock,
    [string]$Pattern,
    [string]$Project,
    [string]$Description
) {
    $allMatches = [regex]::Matches($InitializeBody, $Pattern)
    Assert-True ($allMatches.Count -eq 1) "$Project must set $Description exactly once during startup"
    Assert-True ($CaptureBlock -match $Pattern) "$Project must keep $Description inside the README capture block"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$singleProjects = @(
    '17_fbx_pmx_obj_WithPhong',
    '18_fbx_Animation'
)
$multiProjects = @(
    '19_MultiModels',
    '20_Depth_And_Alpha_Issue',
    '21_MultiModels_With_Animations',
    '23_Rigid_Animation',
    '24_Skinned_With_Bone_Structure'
)
$projects = @($singleProjects) + @($multiProjects)
$expectedProjects = @(
    '17_fbx_pmx_obj_WithPhong',
    '18_fbx_Animation',
    '19_MultiModels',
    '20_Depth_And_Alpha_Issue',
    '21_MultiModels_With_Animations',
    '23_Rigid_Animation',
    '24_Skinned_With_Bone_Structure'
)

Assert-True ($projects.Count -eq 7) 'startup contract must contain exactly seven projects'
Assert-True (@(Compare-Object ($projects | Sort-Object) ($expectedProjects | Sort-Object)).Count -eq 0) 'startup contract target list changed'

$defaultModelCall = 'LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb")'
$capturePattern = 'if\s*\(\s*ReadmeCapture::IsEnabled\(\)\s*\)'
$cameraPositionPattern = 'm_Camera\.SetPosition\(\s*XMFLOAT3\(\s*20\.0f\s*,\s*70\.0f\s*,\s*-150\.0f\s*\)\s*\)'
$cameraRotationPattern = 'm_Camera\.SetRotation\(\s*XMFLOAT3\(\s*10\.0f\s*,\s*-6\.0f\s*,\s*0\.0f\s*\)\s*\)'

foreach ($project in $projects) {
    $sourcePath = Join-Path $repoRoot "Dx11\$project\App.cpp"
    Assert-True (Test-Path -LiteralPath $sourcePath -PathType Leaf) "$project App.cpp missing"
    $source = [IO.File]::ReadAllText($sourcePath)

    $initializeAnchor = $source.IndexOf('bool App::OnInitialize()', [StringComparison]::Ordinal)
    Assert-True ($initializeAnchor -ge 0) "$project App::OnInitialize missing"
    $initializeBody = Get-BracedBlock $source $initializeAnchor "$project App::OnInitialize"

    $loadCallCount = [regex]::Matches($initializeBody, [regex]::Escape($defaultModelCall)).Count
    Assert-True ($loadCallCount -eq 1) "$project must load SampleModel.glb exactly once during startup"

    $guardSuffix = if ($multiProjects -contains $project) {
        '\s*&&\s*!m_->m_Models\.empty\(\)'
    }
    else {
        ''
    }
    $loadConditionPattern = 'if\s*\(\s*' + [regex]::Escape($defaultModelCall) + $guardSuffix + '\s*\)'
    $loadCondition = [regex]::Match($initializeBody, $loadConditionPattern)
    Assert-True $loadCondition.Success "$project default load success guard missing"

    $captureCondition = [regex]::Match($initializeBody, $capturePattern)
    Assert-True $captureCondition.Success "$project README capture gate missing"
    Assert-True ($loadCondition.Index -lt $captureCondition.Index) "$project default model load must happen before the README capture gate"

    $loadBlock = Get-BracedBlock $initializeBody $loadCondition.Index "$project default load block"
    $nestedCaptureCondition = [regex]::Match($loadBlock, $capturePattern)
    Assert-True $nestedCaptureCondition.Success "$project README capture gate must be nested inside the successful default load block"
    $captureBlock = Get-BracedBlock $loadBlock $nestedCaptureCondition.Index "$project README capture block"

    if ($singleProjects -contains $project) {
        $loaderAnchor = $source.IndexOf('bool App::LoadModelFromFile(const std::wstring& pathW)', [StringComparison]::Ordinal)
        Assert-True ($loaderAnchor -ge 0) "$project LoadModelFromFile implementation missing"
        $loaderBody = Get-BracedBlock $source $loaderAnchor "$project LoadModelFromFile"
        Assert-True ($loaderBody -match 'm_->m_modelPos\s*=\s*\{\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\}') "$project loader must reset the single model to the origin"
        Assert-True ($initializeBody -notmatch 'm_->m_modelPos\s*=') "$project startup must not override the loader-provided origin"

        Assert-CaptureOnlySetting $initializeBody $captureBlock 'm_->m_modelScale\s*=\s*XMFLOAT3\(\s*80\.0f\s*,\s*80\.0f\s*,\s*80\.0f\s*\)' $project 'capture model scale'
        Assert-CaptureOnlySetting $initializeBody $captureBlock 'm_->m_modelRotation\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*-35\.0f\s*,\s*0\.0f\s*\)' $project 'capture model rotation'
        Assert-CaptureOnlySetting $initializeBody $captureBlock 'm_->m_RotateModel\s*=\s*true' $project 'capture automatic rotation'
    }
    else {
        $originPattern = 'model\.pos\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\)'
        $originAssignment = [regex]::Match($loadBlock, $originPattern)
        Assert-True $originAssignment.Success "$project must place the loaded model at the origin"
        Assert-True ($originAssignment.Index -lt $nestedCaptureCondition.Index) "$project origin assignment must run outside the README capture gate"

        Assert-CaptureOnlySetting $initializeBody $captureBlock 'model\.scale\s*=\s*XMFLOAT3\(\s*80\.0f\s*,\s*80\.0f\s*,\s*80\.0f\s*\)' $project 'capture model scale'
        Assert-CaptureOnlySetting $initializeBody $captureBlock 'model\.rotDeg\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*-35\.0f\s*,\s*0\.0f\s*\)' $project 'capture model rotation'
        Assert-CaptureOnlySetting $initializeBody $captureBlock 'model\.autoRotate\s*=\s*true' $project 'capture automatic rotation'
    }

    Assert-CaptureOnlySetting $initializeBody $captureBlock $cameraPositionPattern $project 'capture camera position'
    Assert-CaptureOnlySetting $initializeBody $captureBlock $cameraRotationPattern $project 'capture camera rotation'
}

'default character startup contract tests passed'
```

- [ ] **Step 2: Run the new test and verify RED**

Run:

```powershell
pwsh -NoProfile -File .\tools\tests\test_default_character_startup.ps1
```

Expected: FAIL on project 17 with `default model load must happen before the README capture gate`. This demonstrates that the test detects the current normal-startup omission rather than a syntax or fixture problem.

- [ ] **Step 3: Move the 17 and 18 default loads outside the capture gate**

In `Dx11/17_fbx_pmx_obj_WithPhong/App.cpp`, replace the current startup capture block with:

```cpp
	if (LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb"))
	{
		if (ReadmeCapture::IsEnabled())
		{
			m_->m_modelScale = XMFLOAT3(80.0f, 80.0f, 80.0f);
			m_->m_modelRotation = XMFLOAT3(0.0f, -35.0f, 0.0f);
			m_->m_RotateModel = true;
			m_Camera.SetPosition(XMFLOAT3(20.0f, 70.0f, -150.0f));
			m_Camera.SetRotation(XMFLOAT3(10.0f, -6.0f, 0.0f));
		}
	}
```

In `Dx11/18_fbx_Animation/App.cpp`, replace the current startup capture block with:

```cpp
	if (LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb"))
	{
		if (ReadmeCapture::IsEnabled())
		{
			m_->m_modelScale = XMFLOAT3(80.0f, 80.0f, 80.0f);
			m_->m_modelRotation = XMFLOAT3(0.0f, -35.0f, 0.0f);
			m_->m_RotateModel = true;
			m_Camera.SetPosition(XMFLOAT3(20.0f, 70.0f, -150.0f));
			m_Camera.SetRotation(XMFLOAT3(10.0f, -6.0f, 0.0f));
		}
	}
```

Do not add an origin assignment here. Both existing loaders already set `m_->m_modelPos = { 0.0f, 0.0f, 0.0f };` after a successful load.

- [ ] **Step 4: Move the 19, 20, 21, 23, and 24 default loads outside the capture gate**

In `Dx11/19_MultiModels/App.cpp`, replace the current startup capture block with:

```cpp
	if (LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb") && !m_->m_Models.empty())
	{
		auto& model = *m_->m_Models.back();
		model.pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
		if (ReadmeCapture::IsEnabled())
		{
			model.scale = XMFLOAT3(80.0f, 80.0f, 80.0f);
			model.rotDeg = XMFLOAT3(0.0f, -35.0f, 0.0f);
			model.autoRotate = true;
			m_Camera.SetPosition(XMFLOAT3(20.0f, 70.0f, -150.0f));
			m_Camera.SetRotation(XMFLOAT3(10.0f, -6.0f, 0.0f));
		}
	}
```

In `Dx11/20_Depth_And_Alpha_Issue/App.cpp`, replace the current startup capture block with:

```cpp
	if (LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb") && !m_->m_Models.empty())
	{
		auto& model = *m_->m_Models.back();
		model.pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
		if (ReadmeCapture::IsEnabled())
		{
			model.scale = XMFLOAT3(80.0f, 80.0f, 80.0f);
			model.rotDeg = XMFLOAT3(0.0f, -35.0f, 0.0f);
			model.autoRotate = true;
			m_Camera.SetPosition(XMFLOAT3(20.0f, 70.0f, -150.0f));
			m_Camera.SetRotation(XMFLOAT3(10.0f, -6.0f, 0.0f));
		}
	}
```

In `Dx11/21_MultiModels_With_Animations/App.cpp`, replace the current startup capture block with:

```cpp
	if (LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb") && !m_->m_Models.empty())
	{
		auto& model = *m_->m_Models.back();
		model.pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
		if (ReadmeCapture::IsEnabled())
		{
			model.scale = XMFLOAT3(80.0f, 80.0f, 80.0f);
			model.rotDeg = XMFLOAT3(0.0f, -35.0f, 0.0f);
			model.autoRotate = true;
			m_Camera.SetPosition(XMFLOAT3(20.0f, 70.0f, -150.0f));
			m_Camera.SetRotation(XMFLOAT3(10.0f, -6.0f, 0.0f));
		}
	}
```

In `Dx11/23_Rigid_Animation/App.cpp`, replace the current startup capture block with:

```cpp
	if (LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb") && !m_->m_Models.empty())
	{
		auto& model = *m_->m_Models.back();
		model.pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
		if (ReadmeCapture::IsEnabled())
		{
			model.scale = XMFLOAT3(80.0f, 80.0f, 80.0f);
			model.rotDeg = XMFLOAT3(0.0f, -35.0f, 0.0f);
			model.autoRotate = true;
			m_Camera.SetPosition(XMFLOAT3(20.0f, 70.0f, -150.0f));
			m_Camera.SetRotation(XMFLOAT3(10.0f, -6.0f, 0.0f));
		}
	}
```

In `Dx11/24_Skinned_With_Bone_Structure/App.cpp`, replace the current startup capture block with:

```cpp
	if (LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb") && !m_->m_Models.empty())
	{
		auto& model = *m_->m_Models.back();
		model.pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
		if (ReadmeCapture::IsEnabled())
		{
			model.scale = XMFLOAT3(80.0f, 80.0f, 80.0f);
			model.rotDeg = XMFLOAT3(0.0f, -35.0f, 0.0f);
			model.autoRotate = true;
			m_Camera.SetPosition(XMFLOAT3(20.0f, 70.0f, -150.0f));
			m_Camera.SetRotation(XMFLOAT3(10.0f, -6.0f, 0.0f));
		}
	}
```

- [ ] **Step 5: Run the focused tests and verify GREEN**

Run:

```powershell
pwsh -NoProfile -File .\tools\tests\test_default_character_startup.ps1
pwsh -NoProfile -File .\tools\tests\test_visual_capture_contracts.ps1
git diff --check
```

Expected:

- `default character startup contract tests passed`
- `visual capture contract tests passed`
- `git diff --check` exits 0 with no output.

- [ ] **Step 6: Review the exact change scope**

Run:

```powershell
$expected = @(
    'Dx11/17_fbx_pmx_obj_WithPhong/App.cpp',
    'Dx11/18_fbx_Animation/App.cpp',
    'Dx11/19_MultiModels/App.cpp',
    'Dx11/20_Depth_And_Alpha_Issue/App.cpp',
    'Dx11/21_MultiModels_With_Animations/App.cpp',
    'Dx11/23_Rigid_Animation/App.cpp',
    'Dx11/24_Skinned_With_Bone_Structure/App.cpp',
    'tools/tests/test_default_character_startup.ps1'
) | Sort-Object
$actual = @(git diff --name-only) | Sort-Object
if (@(Compare-Object $expected $actual).Count -ne 0) {
    throw "Unexpected changed paths: $($actual -join ', ')"
}
git diff --stat
```

Expected: exactly the seven `App.cpp` files and the new test script. No GLB, `.superpowers/`, project file, README, or capture media path may appear.

- [ ] **Step 7: Commit the startup behavior and regression test**

```powershell
git add -- `
    tools/tests/test_default_character_startup.ps1 `
    Dx11/17_fbx_pmx_obj_WithPhong/App.cpp `
    Dx11/18_fbx_Animation/App.cpp `
    Dx11/19_MultiModels/App.cpp `
    Dx11/20_Depth_And_Alpha_Issue/App.cpp `
    Dx11/21_MultiModels_With_Animations/App.cpp `
    Dx11/23_Rigid_Animation/App.cpp `
    Dx11/24_Skinned_With_Bone_Structure/App.cpp
git diff --cached --check
git commit -m "fix: load default character in tutorial scenes"
```

Expected: one commit containing exactly eight paths.

---

### Task 2: Build and visually verify normal-mode startup

**Files:**

- Verify: `Dx11/TutorialApp.sln`
- Verify: `Dx11/x64/Debug/17_fbx_pmx_obj_WithPhong.exe`
- Verify: `Dx11/x64/Debug/18_fbx_Animation.exe`
- Verify: `Dx11/x64/Debug/19_MultiModels.exe`
- Verify: `Dx11/x64/Debug/20_Depth_And_Alpha_Issue.exe`
- Verify: `Dx11/x64/Debug/21_MultiModels_With_Animations.exe`
- Verify: `Dx11/x64/Debug/23_Rigid_Animation.exe`
- Verify: `Dx11/x64/Debug/24_Skinned_With_Bone_Structure.exe`
- Verify: temporary normal-mode PNG captures outside the repository

**Interfaces:**

- Consumes: the startup behavior and contract test from Task 1, the existing seven Visual Studio projects, `tools/capture_readme_media.ps1`, and `tools/readme_media_manifest.json`.
- Produces: fresh Debug/x64 binaries and visual evidence that each app starts with a character in normal mode; no repository file changes.

- [ ] **Step 1: Re-run source tests from the committed state**

```powershell
pwsh -NoProfile -File .\tools\tests\test_default_character_startup.ps1
pwsh -NoProfile -File .\tools\tests\test_visual_capture_contracts.ps1
```

Expected: both scripts exit 0 and print their pass messages.

- [ ] **Step 2: Build exactly the seven affected projects in Debug/x64**

```powershell
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild -PathType Leaf)) {
    throw "MSBuild not found: $msbuild"
}
$targets = '17_fbx_pmx_obj_WithPhong;18_fbx_Animation;19_MultiModels;20_Depth_And_Alpha_Issue;21_MultiModels_With_Animations;23_Rigid_Animation;24_Skinned_With_Bone_Structure'
& $msbuild .\Dx11\TutorialApp.sln /m "/t:$targets" /p:Configuration=Debug /p:Platform=x64 /verbosity:minimal
if ($LASTEXITCODE -ne 0) { throw "Affected-project build failed with exit code $LASTEXITCODE" }
```

Expected: MSBuild exits 0 and produces all seven named executables. Record any pre-existing warnings separately; there must be zero build errors.

- [ ] **Step 3: Capture each startup scene with README mode explicitly disabled**

Create a temporary manifest rather than changing the repository manifest:

```powershell
$qaRoot = Join-Path ([IO.Path]::GetTempPath()) ('alice-default-character-' + [guid]::NewGuid().ToString('N'))
$qaMedia = Join-Path $qaRoot 'media'
$qaManifest = Join-Path $qaRoot 'readme_media_manifest.normal.json'
New-Item -ItemType Directory -Force -Path $qaMedia | Out-Null

$manifest = Get-Content -Raw -LiteralPath .\tools\readme_media_manifest.json | ConvertFrom-Json
$numbers = @('17', '18', '19', '20', '21', '23', '24')
foreach ($project in @($manifest.projects | Where-Object { $_.number -in $numbers })) {
    $project.readmeCaptureMode = $false
}
$manifest | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $qaManifest -Encoding utf8NoBOM

Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue
foreach ($number in $numbers) {
    pwsh -NoProfile -File .\tools\capture_readme_media.ps1 `
        -Manifest $qaManifest `
        -ProjectNumber $number `
        -SkipGif `
        -OutputDir $qaMedia
    if ($LASTEXITCODE -ne 0) { throw "Normal-mode capture failed for project $number" }
}

Write-Output "Normal-mode QA images: $qaMedia"
Get-ChildItem -LiteralPath $qaMedia -Filter *.png | Sort-Object Name | Select-Object Name, Length
```

Expected: seven non-empty PNG files are produced without setting `DX11_README_CAPTURE`.

- [ ] **Step 4: Inspect all seven normal-mode PNGs**

Open each of these files from `$qaMedia` with the environment's image viewer:

```text
17-fbx-pmx-obj-WithPhong.png
18-fbx-Animation.png
19-MultiModels.png
20-Depth-And-Alpha-Issue.png
21-MultiModels-With-Animations.png
23-Rigid-Animation.png
24-Skinned-With-Bone-Structure.png
```

For every image verify:

- the scene is not empty;
- the Alice default character is rendered;
- there is no obvious immediate loader-error presentation;
- normal-mode framing is used rather than the forced README capture camera.

The source contract proves the world position values are `(0, 0, 0)`; the captures prove the normal execution path actually displays the loaded character.

- [ ] **Step 5: Clean up only the validated temporary QA directory**

```powershell
$resolvedQaRoot = (Resolve-Path -LiteralPath $qaRoot).Path
$resolvedTempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
if (-not ($resolvedQaRoot + '\').StartsWith($resolvedTempRoot, [StringComparison]::OrdinalIgnoreCase) -or
    (Split-Path -Leaf $resolvedQaRoot) -notlike 'alice-default-character-*') {
    throw "Refusing to remove unexpected QA path: $resolvedQaRoot"
}
Remove-Item -LiteralPath $resolvedQaRoot -Recurse -Force
```

Expected: only the generated temporary manifest and captures are removed.

- [ ] **Step 6: Run final verification and confirm repository scope**

```powershell
pwsh -NoProfile -File .\tools\tests\test_default_character_startup.ps1
pwsh -NoProfile -File .\tools\tests\test_visual_capture_contracts.ps1
git diff HEAD^ --check
git show --name-only --format='' HEAD | Sort-Object
git status --short
```

Expected:

- both tests pass freshly;
- the feature commit still contains exactly the seven `App.cpp` files and `tools/tests/test_default_character_startup.ps1`;
- the isolated worktree is clean after the feature commit;
- no model, `.superpowers/`, README, solution, project, or generated media file was committed.

Do not create an empty verification commit. Hand the verified branch to `superpowers:finishing-a-development-branch` for integration choices.
