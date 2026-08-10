# Concise Root README Design

**Date:** 2026-08-10

## Goal

Make only the top of the root `README.md` concise, following the local `D3D11-AliceTutorial-legacy` README. Media recapture and project README rewriting are excluded.

## Changes

- Remove the root `README-BRAND` logo block.
- Remove the archived-README notice.
- Keep the two-line project description, environment list, and YouTube/Velog table.
- Remove the long `대표 데모` and `코드 구조 요약` sections.
- Start `프로젝트 바로가기` immediately after the link table.
- Keep the project gallery and every section from `빌드 방식` downward unchanged.
- Keep the 37 project README logo blocks and all existing PNG/GIF/info media unchanged.

## Automation

`tools/update_readme_branding.ps1` will stop targeting the root README and continue maintaining only the 37 project README logo blocks. Branding tests will require no root logo and retain all project-logo and executable-icon checks.

## Verification

- Root README contains no `README-BRAND` marker or `alice-tutorial-logo.png` reference.
- Root README contains no `채용자`, `대표 데모`, or `코드 구조 요약` text.
- All 37 project README branding contracts still pass.
- README media links and existing media files remain unchanged.
- Existing user changes in `Camera.cpp` and character GLBs remain unstaged and uncommitted.
