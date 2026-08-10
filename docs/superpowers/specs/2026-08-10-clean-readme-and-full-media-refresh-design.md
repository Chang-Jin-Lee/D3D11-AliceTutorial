# Clean README and Full Media Refresh Design

**Date:** 2026-08-10

## Goal

Refresh the evidence media for all 37 manifest projects using the current local character resources, simplify the README presentation to match the legacy repository's restrained style, remove README logos, and push the verified result to `main`.

## README scope

### Root README

The top of `README.md` will follow the local `D3D11-AliceTutorial-legacy` README structure:

1. Repository title
2. Two-line project description
3. Short environment and purpose list
4. YouTube and Velog links with current project media
5. Project directory gallery

The following presentation-heavy material will be removed:

- the `README-BRAND` logo block;
- the archived-public-README notice at the top;
- the recruiter-oriented representative-demo paragraph;
- the separate representative-demo comparison table;
- the representative-demo implementation summary table;
- the `코드 구조 요약` section.

Build instructions, dependency notes, model-format notes, cautions, resource links, references, and license information remain. Their technical facts are not rewritten.

### Project READMEs

- Remove every generated `README-BRAND` logo block from the 37 manifest project READMEs.
- Keep navigation, information image, project-specific explanation, runtime PNG/GIF, and navigation footer.
- Replace only explicit audience/portfolio wording and clearly awkward sentences.
- Preserve code, filenames, API names, numerical values, technical claims, and the original register.

Initial known wording changes:

| Location | Before | After intent |
|---|---|---|
| Root representative demo | `채용자가 빠르게 볼 대표 프로젝트는 ...` | Remove the section; the project gallery already links the demo. |
| Root dependency table | `포트폴리오 주제에서 벗어날 만큼 작업량이 큽니다.` | Explain that the implementation cost is outside this tutorial's learning scope. |
| Project 35 heading | `포트폴리오에서 볼 포인트` | Rename to `구현 확인 항목`. |

All additional prose edits require a concrete context or grammar defect. The implementation report will list every before/after pair and its reason. Broad stylistic rewriting is out of scope.

The Korean edits follow the local `C:\Github\im-not-ai` fast-path rules: remove only mapped AI-like patterns, preserve meaning and technical terms, avoid added hype or stock phrases, and keep the change rate low.

## Media refresh

`tools/readme_media_manifest.json` remains the source of truth for exactly 37 projects.

For every project:

1. Build the Debug x64 executable.
2. Launch it with the current main-checkout resources.
3. Capture a fresh 1600x900 runtime PNG.
4. Capture or generate a fresh 800x450 GIF.
5. Regenerate the 1600x640 information image from the new PNG.
6. Record the result in the capture report.

The existing full-window composition remains. Project-specific capture actions may be adjusted only when a model, scene, UI, or animation is not readable. Static scenes may use the existing reproducible presentation-pan GIF treatment; animated scenes must show real frame changes.

If desktop capture remains unavailable to the Codex shell, an environment-gated temporary backbuffer capture hook may be used. Temporary source changes must be restored byte-for-byte before staging, and the final tracked source diff must contain no capture hook.

## Visual and automated acceptance

- 37/37 runtime PNG files exist, are 1600x900, nonblank, and show the intended project scene.
- 37/37 GIF files exist, are 800x450, nonblank, and pass motion checks appropriate to the scene.
- 37/37 information images exist, are 1600x640, and embed the current PNG rather than an older capture.
- Project 23 visibly shows BoxHuman at position `0,0,0`, scale `0.01,0.01,0.01`, with playback enabled and rigid animation changing frames.
- Aggregate review sheets contain 37 distinct, readable project tiles with no capture-tool windows or blank DirectX surfaces.
- The README media verifier and all related contract tests pass.
- The full `TutorialApp.sln` Debug x64 rebuild succeeds.

## Safety and Git scope

Before capture, record SHA-256 hashes for the current user-owned or protected files:

- `Dx11/Common/Camera.cpp`
- `Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb`
- `Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel_SwimSuit.glb`
- `Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel_kimno.glb`
- `Dx11/Resource/fbx/Study/BoxHuman.fbx`

Recheck the hashes after capture. Existing user changes and untracked GLBs remain outside the commits.

Commits will separate README/tooling changes from generated media where practical. After fresh verification, push local `main` to `origin/main`. No force push is permitted.

## Reporting

The final report will include:

- the 37-project capture result summary;
- visual QA failures and any project-specific retries;
- every README prose change as before/after text with a reason;
- removed logo-block counts;
- test, media-verifier, build, protected-hash, commit, and push results.
