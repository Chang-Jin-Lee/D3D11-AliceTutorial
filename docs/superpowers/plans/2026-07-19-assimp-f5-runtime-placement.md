# Assimp F5 Runtime Placement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the tracked x64 Assimp runtime available beside Debug, Release, and common-bin executables immediately after `git pull`, so Visual Studio F5 works even when it skips a rebuild.

**Architecture:** Keep `Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll` as the canonical binary and materialize identical tracked copies in the three executable locations. Narrow `.gitignore` exceptions expose only those DLLs, while the existing MSBuild target continues refreshing output copies after real builds.

**Tech Stack:** Git ignore rules, PowerShell regression checks, MSBuild/Visual C++ output conventions, Windows x64 VC143 Assimp runtime

## Global Constraints

- Cover x64 Debug and x64 Release only; do not add or claim Win32 runtime support.
- Track exactly `Dx11/bin/assimp-vc143-mt.dll`, `Dx11/x64/Debug/assimp-vc143-mt.dll`, and `Dx11/x64/Release/assimp-vc143-mt.dll` in addition to the canonical DLL.
- Keep every other build artifact in `Dx11/bin` and `Dx11/x64` ignored.
- Keep `CopyThirdPartyRuntimeDlls` in `Dx11/Directory.Build.targets` unchanged.
- Require every tracked output copy to have the same SHA-256 hash as the canonical DLL.
- Do not launch tutorial executables; the user performs runtime testing on the other computer.
- Commit and push the completed change to `main`, incorporating newly fetched remote `main` commits without force-pushing.

---

## File map

- `.gitignore`: narrowly unignore the three Assimp runtime output copies.
- `tools/tests/test_portable_runtime.ps1`: enforce existence, tracking, ignore state, and SHA-256 equality.
- `Dx11/bin/assimp-vc143-mt.dll`: common-bin runtime.
- `Dx11/x64/Debug/assimp-vc143-mt.dll`: Visual Studio x64 Debug runtime.
- `Dx11/x64/Release/assimp-vc143-mt.dll`: Visual Studio x64 Release runtime.
- `Dx11/third_party/README.md`: explain pull-time and build-time runtime placement.

### Task 1: Track identical Assimp runtimes at every x64 executable location

**Files:**
- Modify: `tools/tests/test_portable_runtime.ps1:37-66`
- Modify: `.gitignore:20-39`
- Create: `Dx11/bin/assimp-vc143-mt.dll`
- Create: `Dx11/x64/Debug/assimp-vc143-mt.dll`
- Create: `Dx11/x64/Release/assimp-vc143-mt.dll`
- Source: `Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll`

**Interfaces:**
- Consumes: canonical x64 DLL at `Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll`.
- Produces: three tracked, byte-identical output copies and a regression check that rejects missing, ignored, untracked, empty, or stale copies.

- [ ] **Step 1: Write the failing output-placement regression test**

Immediately after the canonical DLL tracking assertion in `tools/tests/test_portable_runtime.ps1`, add:

~~~powershell
$assimpOutputRelatives = @(
    'Dx11/bin/assimp-vc143-mt.dll',
    'Dx11/x64/Debug/assimp-vc143-mt.dll',
    'Dx11/x64/Release/assimp-vc143-mt.dll'
)

$canonicalAssimpHash = $null
if (Test-Path -LiteralPath $assimpDll -PathType Leaf) {
    $canonicalAssimpHash = (Get-FileHash -LiteralPath $assimpDll -Algorithm SHA256).Hash
}

foreach ($outputRelative in $assimpOutputRelatives) {
    $outputDll = Join-Path $repoRoot $outputRelative
    Assert-True -Condition (Test-Path -LiteralPath $outputDll -PathType Leaf) `
        -Message "Assimp output runtime DLL is missing: $outputRelative"

    & git -C $repoRoot check-ignore --quiet -- $outputRelative
    $outputIgnoredExitCode = $LASTEXITCODE
    Assert-True -Condition ($outputIgnoredExitCode -ne 0) `
        -Message "Assimp output runtime DLL is still ignored by Git: $outputRelative"

    $trackedOutputFiles = @(& git -C $repoRoot ls-files -- $outputRelative)
    Assert-True -Condition ($trackedOutputFiles -contains $outputRelative) `
        -Message "Assimp output runtime DLL is not tracked by Git: $outputRelative"

    if (Test-Path -LiteralPath $outputDll -PathType Leaf) {
        Assert-True -Condition ((Get-Item -LiteralPath $outputDll).Length -gt 0) `
            -Message "Assimp output runtime DLL is empty: $outputRelative"
        if ($canonicalAssimpHash) {
            $outputHash = (Get-FileHash -LiteralPath $outputDll -Algorithm SHA256).Hash
            Assert-True -Condition ($outputHash -eq $canonicalAssimpHash) `
                -Message "Assimp output runtime DLL differs from the canonical DLL: $outputRelative"
        }
    }
}
~~~

- [ ] **Step 2: Run the test and verify RED**

Run:

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
~~~

Expected: exit `1`. Each existing output copy reports both `is still ignored by Git` and `is not tracked by Git`. A missing local copy also reports `is missing`. Existing canonical DLL and Live2D checks remain green.

- [ ] **Step 3: Add narrow ignore exceptions**

Replace the current Assimp exception block in `.gitignore` with:

~~~gitignore
# Repo-local Assimp runtime required by model-loading samples.
!Dx11/third_party/assimp/bin/
!Dx11/third_party/assimp/bin/msvc/
!Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll

# Keep the runtime beside already-built x64 executables after git pull.
!Dx11/bin/
Dx11/bin/*
!Dx11/bin/assimp-vc143-mt.dll
!Dx11/x64/
Dx11/x64/*
!Dx11/x64/Debug/
Dx11/x64/Debug/*
!Dx11/x64/Debug/assimp-vc143-mt.dll
!Dx11/x64/Release/
Dx11/x64/Release/*
!Dx11/x64/Release/assimp-vc143-mt.dll
~~~

- [ ] **Step 4: Materialize identical output copies**

Run from the repository root:

~~~powershell
$canonicalDll = Resolve-Path 'Dx11\third_party\assimp\bin\msvc\assimp-vc143-mt.dll'
$outputDirs = @('Dx11\bin', 'Dx11\x64\Debug', 'Dx11\x64\Release')
foreach ($outputDir in $outputDirs) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
    Copy-Item -LiteralPath $canonicalDll -Destination (Join-Path $outputDir 'assimp-vc143-mt.dll') -Force
}
~~~

Expected: all three destination files exist and are `1,752,064` bytes.

- [ ] **Step 5: Stage only planned output files**

Run:

~~~powershell
git add -- .gitignore tools/tests/test_portable_runtime.ps1 Dx11/bin/assimp-vc143-mt.dll Dx11/x64/Debug/assimp-vc143-mt.dll Dx11/x64/Release/assimp-vc143-mt.dll
git status --short
~~~

Expected: the two text files and three DLL paths appear. No EXE, PDB, OBJ, or other output appears.

- [ ] **Step 6: Run the regression test and verify GREEN**

Run:

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
~~~

Expected: exit `0` and `Portable runtime verification passed.`

- [ ] **Step 7: Verify ignore isolation and Git blob identity**

Run:

~~~powershell
git ls-files --stage -- Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll Dx11/bin/assimp-vc143-mt.dll Dx11/x64/Debug/assimp-vc143-mt.dll Dx11/x64/Release/assimp-vc143-mt.dll
git check-ignore --quiet -- Dx11/x64/Debug/05_Mesh.exe
$LASTEXITCODE
~~~

Expected: four DLL lines use blob ID `9c27d6384a1e27d8e831f79466cefbd8a5ddd291`, and the EXE check prints `0` because non-DLL output stays ignored.

- [ ] **Step 8: Commit the runtime placement fix**

Run:

~~~powershell
git commit -m "fix: place Assimp runtime beside x64 executables"
~~~

Expected: one commit containing the test, `.gitignore`, and three runtime paths only.

### Task 2: Document the pull-and-F5 runtime contract

**Files:**
- Modify: `tools/tests/test_portable_runtime.ps1:153-159`
- Modify: `Dx11/third_party/README.md:9-16`

**Interfaces:**
- Consumes: the tracked output paths from Task 1.
- Produces: documentation distinguishing pull-time copies from build-time refreshed copies.

- [ ] **Step 1: Add a failing documentation contract**

Add after the existing third-party README assertions:

~~~powershell
Assert-Contains -Text $thirdPartyReadme -Expected 'Dx11/x64/Debug' `
    -Message 'third_party README does not document the tracked Debug runtime copy.'
Assert-Contains -Text $thirdPartyReadme -Expected 'Dx11/x64/Release' `
    -Message 'third_party README does not document the tracked Release runtime copy.'
Assert-Contains -Text $thirdPartyReadme -Expected 'Visual Studio skips a rebuild' `
    -Message 'third_party README does not explain the pull-and-F5 runtime contract.'
~~~

- [ ] **Step 2: Run the test and verify RED**

Run the portable-runtime test. Expected: exit `1` with exactly the three new README failures; Task 1 checks remain green.

- [ ] **Step 3: Document both placement mechanisms**

Replace the current one-line Assimp copy explanation in `Dx11/third_party/README.md` with:

~~~markdown
`assimp-vc143-mt.dll` is also tracked at `Dx11/bin`, `Dx11/x64/Debug`, and
`Dx11/x64/Release`. These pull-time copies keep existing x64 executables
launchable with F5 even when Visual Studio skips a rebuild.

`Directory.Build.targets` refreshes the same DLL from
`third_party/assimp/bin/msvc` to each executable output directory and to
`Dx11/bin` after a successful build.
~~~

- [ ] **Step 4: Verify GREEN and whitespace**

Run:

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
git diff --check
~~~

Expected: test exit `0` with `Portable runtime verification passed.` and no output from `git diff --check`.

- [ ] **Step 5: Commit the documentation contract**

Run:

~~~powershell
git add -- tools/tests/test_portable_runtime.ps1 Dx11/third_party/README.md
git commit -m "docs: explain Assimp pull-time runtime copies"
~~~

### Task 3: Verify, integrate remote main, and push

**Files:**
- Verify only: all files changed by Tasks 1 and 2.

**Interfaces:**
- Consumes: approved design/plan and two implementation commits.
- Produces: clean local `main` synchronized with `origin/main`.

- [ ] **Step 1: Run fresh requirement verification**

Run:

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
git diff --check origin/main..HEAD
~~~

Expected: test exit `0`, `Portable runtime verification passed.`, and no diff-check output.

- [ ] **Step 2: Review the delivery set**

Run:

~~~powershell
git status --short --branch
git log --oneline --decorate origin/main..HEAD
git diff --stat origin/main..HEAD
~~~

Expected: clean `main` and only planned design, plan, test, ignore, DLL, and documentation changes.

- [ ] **Step 3: Fetch and incorporate remote changes safely**

Run:

~~~powershell
git fetch origin
git status --short --branch
~~~

Expected: `main` is ahead and not behind. If behind, run `git merge --no-edit origin/main`, resolve genuine overlaps only, and rerun Steps 1 and 2. Never force-push.

- [ ] **Step 4: Push current main**

Run `git push origin main`. Expected: `main -> main` succeeds.

- [ ] **Step 5: Verify synchronization**

Run:

~~~powershell
git status --short --branch
git rev-parse HEAD
git rev-parse origin/main
~~~

Expected: `## main...origin/main`, otherwise empty status, and identical SHAs.

