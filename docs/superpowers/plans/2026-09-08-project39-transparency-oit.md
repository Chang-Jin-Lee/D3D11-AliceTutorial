# Project 39 Transparency OIT implementation plan

> Execute with `superpowers:subagent-driven-development` in this session. The user approved starting the proposed project; do not add an approval pause.

Goal: deliver a runnable Direct3D 11 transparency comparison lesson with the author's VRM-exported character, real GPU behavior tests, and captured documentation.

Architecture: an independent `OitPipeline` owns render targets, OM states, fullscreen resolve/presentation, and asynchronous timings. `App` owns fixed-pose character/quad geometry, surface shaders, draw classification/sorting, camera, and UI. WARP tests directly compile the production pipeline and HLSL.

Tech stack: existing Visual Studio v143/C++20, D3D11/Shader Model 5, DirectXTK, Assimp and ImGui. No new dependency.

Spec: `docs/superpowers/specs/2026-09-08-project39-transparency-oit-design.md` (binding interface and behavior).

## Global Constraints

- Work only on `codex/project39-transparency-oit`. Do not merge or push this feature to main. Prior work is already pushed as `b3d4ba5`.
- Do not modify Common, Common/Animation, existing rendering code, models, textures, or third-party libraries. No symlinks/junctions. Preserve unrelated `.superpowers/` content.
- Use `apply_patch` for source/document edits. Tests exercise behavior, not source-regex patterns. Show a relevant failing test before the corresponding implementation.
- Keep glTF alpha semantics and baseColorFactor intact. Only the explicitly validated lace material can be promoted by its experiment toggle. Transparent passes must not write depth.
- The same OitPipeline and shared HLSL must run in app and WARP tests. FP16 tolerances are explicit; OIT is approximate, not exact sorted OVER.
- Shader/model initialization failure must be visible. GPU timing queries are nonblocking and optional. Linear blending precedes a single display conversion.
- Native process launch must normalize duplicate PATH/Path environment entries. PowerShell tools use `login:false`. Builds use MSBuild 2022 BuildTools, x64 Debug, `/m:1 /nr:false /p:BuildInParallel=false` to avoid worker launch and shared-PCH problems.
- Implementation agents own their listed files only, never dispatch subagents, and do not commit, stage, switch, merge or push. The controller owns git integration after review. Each agent writes its report under this plan's SDD workspace.

## Task 1: Test and implement the reusable OIT render passes

Files to create:

- `Dx11/39_Transparency_OIT/OitPipeline.h` and `.cpp`
- `Dx11/39_Transparency_OIT/39_Transparency.fxh`
- `Dx11/39_Transparency_OIT/39_Resolve.hlsl`
- `tools/tests/native/OitPipelineTests.cpp`, `OitPipelineTests.vcxproj`, `OitTestSurface.hlsl`
- `tools/tests/test_oit_pipeline.ps1`

Read the spec's full rendering contract and exact public interface; implement that interface verbatim. The pipeline must not depend on Common, ImGui or Assimp. It owns HDR/Accumulation/Revealage/final HDR/depth textures, blend/depth states and fullscreen shaders. BeginOpaque clears HDR/depth and binds depth-write state. BeginTransparency configures AlphaTest, SortedBlend or independent-MRT OIT, and clears OIT buffers every frame (including non-OIT frames so debug views never show stale data). EndTransparency closes timing. Resolve produces LinearTexture for every mode. Present supports Composite, Accumulation (normalized weighted color) and Revealage (grayscale) and unbinds inputs. Provide texture/SRV getters from the spec and useful HRESULT diagnostics without changing signatures.

The shared HLSL names should be `OitOutput` with `float4 accumulation : SV_Target0` and `float revealage : SV_Target1`, and `MakeOitOutput(float3 linearColor, float alpha, float normalizedViewDepth)`. Clamp alpha, discard alpha zero, clamp HDR contribution to [0,8], use the spec weight, and output weighted premultiplied color/alpha and source alpha for revealage. Surface shaders supply normalized *linear view depth*, not hardware z. Resolve must guard empty and nonfinite accumulations. State descriptors initialize all active fields. Use independent blending with revealage ZERO / INV_SRC_COLOR (not INV_SRC_ALPHA on a scalar output).

GPU timings use a small ring of disjoint/timestamp queries, poll with DONOTFLUSH without waiting, never overwrite unresolved in-flight slots, and expose available/valid/transparentMs/compositeMs. Composite measures Resolve, not Present/UI. Unavailable timings do not fail rendering. Resize rejects zero dimensions and handles allocation failure safely. Minimize handling belongs to App.

TDD:

1. Create a WARP executable fixture rendering real clip-space geometry to the production pipeline and mapping staging textures. The test surface shader includes the production shared HLSL via a relative include. First run `pwsh -NoProfile -File tools/tests/test_oit_pipeline.ps1` with the new expectations and minimal missing/stub pipeline; record the failure.
2. Implement passes. Assert: empty transparency preserves HDR background; single layer matches analytical OVER within 0.003; two differently colored layers reversed give OIT within 0.005 with revealage product within 0.002, while reversed ordinary OVER differs by >0.05; opaque depth rejects rear transparency; alpha-zero writes neither color nor depth; alpha-test threshold below/above cases write depth only when retained; resize changes texture size and still renders; Present Composite/Accumulation/Revealage yield finite expected output. Add a dense-layer/high-color finite-result check for resolve guarding.
3. Compile real shaders with warnings-as-errors, test native code /W4 /WX, no source text contract tests. Test project links d3d11/dxgi/d3dcompiler only and production OitPipeline.cpp; no Common reference. Wrapper follows native test process normalization patterns, locates MSBuild, uses a GUID temp directory and validates it before recursive cleanup. Capture the relevant RED/GREEN output in report.
4. Run focused suite once green, inspect diff, report interface decisions and tests. Do not commit; controller packages the task diff for review.

## Task 2: Build the fixed-pose character and intersecting-surface comparison app

Files to create:

- `Dx11/39_Transparency_OIT/App.h`, `App.cpp`, `WinMain.cpp`
- `Dx11/39_Transparency_OIT/SceneTransparency.h`, `SceneTransparency.cpp`
- `Dx11/39_Transparency_OIT/39_Surface.hlsl`
- `Dx11/39_Transparency_OIT/39_Transparency_OIT.vcxproj`, `.vcxproj.filters`
- `tools/tests/native/SceneTransparencyTests.cpp`, `SceneTransparencyTests.vcxproj`
- `tools/tests/test_scene_transparency.ps1`

Read the spec and Task 1's header/shared shader (available by dispatch). Do not edit Task 1 files. Derive App from existing GameApp; inspect project38 for device, resize, ImGui and model-loading conventions. Do not copy its toon/outline/shadow implementation. Use `FbxModel` to load `..\Resource\fbx\Public\MyAlice\Player\SampleModel.glb`, preserve authored base color and alpha (inspect whether shared loader bakes factors before multiplying again), select Idle if available and evaluate a fixed 0.5-second pose once. Bone palette and centroid transforms must match that same pose. Simple fixed directional diffuse + ambient lighting is sufficient; this is not another PBR lesson.

Create geometry for three intersecting translucent tinted planes plus a small opaque foreground occluder and rear background plane. Use meters and frame the character without cropping. Provide Both/Character/Planes scenes, full-body/lace close-up camera, optional orbit. Pipeline defaults WeightedOit, lace experiment defaults off, alpha multiplier 1, experiment cutoff 0.5, exposure 1. Key bindings and controls must match the spec. Turning on lace experiment validates both material index/name; if mismatch disable it visibly. The GLB JSON was inspected: exact material 4 name is `N00_002_01_Tops_01_CLOTH_02 (Instance)`, including suffix. Model bounds are about x[-0.64,0.64], y[0,1.55]; lace y[0.37,1.0]. It has T-Pose and VRM_1 through VRM_7 clips, no literal Idle; follow project38's VRM idle fallback if no Idle. Mode AlphaTest converts BLEND draw entries to cutouts with the experiment cutoff, preserving original MASK cutoff. Alpha multiplier only affects lace experiment and diagnostic planes, not eye materials. World/view/projection and skinned layout must match Common/Vertex.h; model textures' color-space path must be respected.

`SceneTransparency` is a small pure helper for effective material mode/cutoff/opacity and global stable back-to-front ordering; use it in App and native tests. Write failing tests first for original glTF modes, guarded lace override, original MASK cutoff, AlphaTest BLEND conversion, targeted opacity, ordering with ties and reverse. No Assimp or GPU dependency for this helper. Add the tests' build wrapper using the established environment-safe launch pattern.

App creates device/swapchain with explicit error handling, uses Pipeline for every rendering mode, and builds one global transparent list across character subsets and planes. Display CPU sort duration (sorting only, 0/not needed in OIT) and pipeline timing readiness/status. Keep geometry/lighting/pose identical between mode switches. Handle WM_SIZE, minimize, lost/failed Present visibly, and release resources without leaking ImGui state. Honor DX11_README_CAPTURE; captures remain fixed unless an input explicitly turns orbit on. UI must not cover the character or lace in default/close-up views.

Create the vcxproj with GUID `{D925C03C-9331-47A7-98D5-17975AF40F39}`; match existing v143/C++20 x64 Debug/Release and Common project reference `{05774CF5-5EB5-455B-8ADF-707FB11F2F9F}`. Include/copy both Task 1 shaders and new surface shader as runtime files, no legacy resource changes. Match resource copy behavior and allow direct project build with `SolutionDir` set to Dx11. Controller handles solution/branding/manifest in Task 3.

Run `pwsh -NoProfile -File tools/tests/test_scene_transparency.ps1` RED then GREEN, existing OIT GPU suite after integration, and MSBuild2022 for the new vcxproj x64 Debug with normalized environment and `/m:1 /p:BuildInParallel=false /nr:false`. Record build/test evidence, fixed pose and material mapping details. Do not commit. Report paths and any runtime QA still needed.

## Task 3: Integrate the lesson and document observed behavior

Files to modify/create:

- `Dx11/TutorialApp.sln`, `Dx11/Directory.Build.targets`
- `tools/readme_media_manifest.json`
- `tools/readme_media_common.ps1`, `tools/capture_readme_media.ps1`
- `tools/tests/test_readme_media_manifest.ps1`, `tools/tests/test_capture_manifest_actions.ps1`
- `Dx11/39_Transparency_OIT/README.md`, `Dx11/38_StylizedToonPBR/README.md`
- root `README.md`, `docs/project39-transparency-proposal.md`
- generated `docs/media/readme/39-Transparency-OIT.png`, `.gif`, `info/39-Transparency-OIT-info.png`, and `39-Transparency-OIT-{alpha-test,sorted,oit}-lace.png`

Register the Task 2 GUID in every solution configuration and add the project to AliceTutorialBrandingProjects. Add manifest entry 39 (`directory`/`exe` from project name, `image`/`gif`/`infoImage` from file map), change expected count to 39, use capture mode and a real runtime GIF with keyboard actions enabling lace and orbit. Author Korean README using existing NAV/INFO/RUNTIME markers in correct order. Include learning goal, relation to 20/35/38, implementation code links, the exact buffers/blend/resolve formula and normalized linear depth weight, controls, original glTF vs experiment semantics, nonblocking timing interpretation, approximation/FP16/multisampling limitations and real captures. Cite the primary sources linked in the spec. Do not invent performance numbers or claim visual differences before observing.

The existing capture tool accepts WASD only. Extend its explicit virtual-key map and manifest validator only to the lesson's extra keys `1,2,3,L,R,O,C` (S is already accepted); use the same allowed-key source for validation and dispatch if practical. Write failing behavioral tests before implementation: the added keys validate and dispatch their exact WM_KEYDOWN/WM_KEYUP virtual key codes through the existing MessageSink seam; unsupported keys still fail validation. Add 39 to the explicit capture-mode project selection fixture. Do not replace behavior tests with source regex. A pre-capture tap must have keyDown, a wait long enough to cross an app update (at least 125 ms), then keyUp; back-to-back messages can miss the edge. The GIF action schedule should hold O across separate encoded frames.

Update proposal's opening to link the now-implemented lesson and 38's next-topic link to the real README. Use existing info/media/update scripts (inspect their parameters first) to generate navigation/catalog/infocard. Capture PNG/GIF from the running app, not synthetic images. Actual lace comparison captures must each switch to their labeled mode with identical pose/light/camera. Controller may perform interactive capture/visual QA while the agent authors/integrates text; agree ownership of binary artifacts explicitly.

Validation: new native GPU + scene suites; existing README updater fixtures, manifest contracts, media verifier; local Markdown target check; `git diff --check`; x64 Debug and Release new-project builds. Inspect actual rendered PNG and GIF frames with image tools; correct real visual defects through the appropriate implementation agent. Report what was actually executed and what remains a limitation. Do not commit or push; controller performs final reviewed checkpoint.
