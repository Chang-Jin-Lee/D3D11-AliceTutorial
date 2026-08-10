# Concise Root README Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Simplify only the root README top, remove its logo, preserve all project README/media content, and push the verified result.

**Architecture:** Change the branding contract first so the root README is intentionally logo-free while the 37 manifest project READMEs retain their current logo blocks. Then remove the redundant root introduction sections and verify that no media or user-owned source/resource file entered the diff.

**Tech Stack:** Markdown, PowerShell 7, Git

## Global Constraints

- Do not recapture or modify PNG, GIF, or information images.
- Do not rewrite project README prose or remove project README logos.
- Do not stage or commit `Dx11/Common/Camera.cpp`, character GLBs, or `.superpowers/`.
- Use a normal `main` push only; never force push.

---

### Task 1: Require a logo-free root README

**Files:**
- Modify: `tools/tests/test_update_readme_branding.ps1`
- Modify: `tools/tests/test_app_branding.ps1`
- Modify: `tools/tests/test_app_branding_acceptance.ps1`
- Modify: `tools/update_readme_branding.ps1`

**Interfaces:**
- Consumes: the 37 manifest directories.
- Produces: an updater that targets only project READMEs and contracts that reject a root logo while preserving 37 project logo blocks.

- [ ] **Step 1: Change tests first**

In the updater fixture, make the root README `# Root\r\n\r\n> Root quote\r\n` and assert it is byte-identical after two updater runs. Keep project insertion, movement, idempotence, encoding, malformed-marker, and manifest-safety assertions.

In `test_app_branding.ps1`, remove the root entry from the `$readmes` branding list, require exactly 37 project entries, and add:

```powershell
$rootReadme = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'README.md')
Assert-True ($rootReadme -notmatch 'README-BRAND:(?:START|END)') 'root README brand markers must be absent'
Assert-True ($rootReadme -notmatch 'alice-tutorial-logo\.png') 'root README logo reference must be absent'
```

In the acceptance fixture, write the root README without a brand block, keep project fixtures branded, remove the root brand-spacing negative case, and add a case that inserts a root brand block and expects `root README brand markers must be absent`.

- [ ] **Step 2: Run tests and verify RED**

```powershell
pwsh -NoProfile -File .\tools\tests\test_update_readme_branding.ps1
pwsh -NoProfile -File .\tools\tests\test_app_branding.ps1
pwsh -NoProfile -File .\tools\tests\test_app_branding_acceptance.ps1
```

Expected: updater/root branding assertions fail because production and the real root README still include the root logo.

- [ ] **Step 3: Stop the updater from targeting root README**

Remove this target from `tools/update_readme_branding.ps1`:

```powershell
$targets.Add([pscustomobject]@{ Path = Join-Path $root 'README.md'; Image = 'docs/media/branding/alice-tutorial-logo.png'; Width = 720 })
```

Keep the 37 project targets and all manifest/solution safety checks unchanged.

### Task 2: Simplify the root README top

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: the legacy README opening.
- Produces: a short root introduction followed directly by the current project gallery.

- [ ] **Step 1: Remove the root-only presentation blocks**

Delete the `README-BRAND` block, archived-README blockquote, `대표 데모` section, and `코드 구조 요약` section. Keep this order:

```markdown
# D3D11-AliceTutorial

이 저장소는 [DirectX SDK Samples - Direct3D11 Tutorials](https://github.com/walbourn/directx-sdk-samples/tree/main/Direct3D11Tutorials) 을 기반으로<br>
D3D 그래픽스를 학습하면서 정리한 튜토리얼 프로젝트입니다.

- 환경: Windows 11, Visual Studio 2022
- 플랫폼: Win32 Desktop (Direct3D 11.0)
- 목적: DirectX 11 그래픽스 파이프라인의 기초 학습 및 3D 기능 탐구
```

Keep the existing YouTube/Velog table. Place `### 프로젝트 바로가기` and the existing gallery directly after it. Do not change anything from the gallery through the end of the README.

- [ ] **Step 2: Run focused content checks**

```powershell
rg -n "README-BRAND|alice-tutorial-logo|채용자|^## 대표 데모|^### 코드 구조 요약" README.md
```

Expected: no matches.

### Task 3: Verify, commit, and push

**Files:**
- Verify/commit: `README.md`, three branding tests, branding updater, updated spec, and this plan

**Interfaces:**
- Consumes: Tasks 1-2.
- Produces: verified remote `main` without media or protected user changes.

- [ ] **Step 1: Run relevant tests**

```powershell
$tests = @(
  '.\tools\tests\test_update_readme_branding.ps1',
  '.\tools\tests\test_app_branding.ps1',
  '.\tools\tests\test_app_branding_acceptance.ps1',
  '.\tools\tests\test_project_readme_updater.ps1',
  '.\tools\tests\test_verify_readme_media.ps1'
)
foreach ($test in $tests) {
  & pwsh -NoProfile -File $test
  if ($LASTEXITCODE -ne 0) { throw "$test failed: $LASTEXITCODE" }
}
pwsh -NoProfile -File .\tools\verify_readme_media.ps1 -Manifest tools/readme_media_manifest.json
```

Expected: five tests and the actual media-link verifier exit 0.

- [ ] **Step 2: Verify exact diff scope**

```powershell
git diff --check
git status --short
git diff --name-only
```

Expected tracked changes are limited to the root README, updater/tests, spec, and plan. No media file, project README, source file, or GLB is staged.

- [ ] **Step 3: Commit exact files**

```powershell
git add -- README.md tools/update_readme_branding.ps1 tools/tests/test_update_readme_branding.ps1 tools/tests/test_app_branding.ps1 tools/tests/test_app_branding_acceptance.ps1 docs/superpowers/specs/2026-08-10-clean-readme-and-full-media-refresh-design.md docs/superpowers/plans/2026-08-10-concise-root-readme.md
git diff --cached --check
git commit -m "docs: simplify root README introduction"
```

- [ ] **Step 4: Push normally**

```powershell
git fetch origin
git rev-list --left-right --count origin/main...main
git push origin main
git rev-list --left-right --count origin/main...main
```

Expected: origin has no unintegrated commits before push and divergence is `0 0` afterward.
