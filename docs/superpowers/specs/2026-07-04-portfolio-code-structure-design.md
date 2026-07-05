# Portfolio Code Structure Design

## Goal

Improve the portfolio readability of `D3D11-AliceTutorial` without changing the visible demo behavior or turning the tutorial code into a heavy engine rewrite.

The work focuses on the issues called out in the portfolio review:

- late-stage samples look too much like accumulated tutorial code
- `Dx11/36_AdvancedAnim_Sound_Click/App.cpp` is too large to scan quickly
- `Dx11/35_DeferredRendering` has no README
- the root README needs a clearer "representative demo" entry point

## Current Context

The reference project at `C:\Github\dx11-math-shader` is simple and easy to read because its structure exposes intent:

- `Scene/SceneXX_*.cpp` for feature demos
- `Render/` for render helpers
- `Math/` for pure math helpers
- README sections that summarize build, controls, architecture, and demo evidence

This repository already has shared code in `Dx11/Common`, but the final demo `36_AdvancedAnim_Sound_Click` still concentrates lifecycle, render passes, model loading, UI, sound, and HDR/deferred state in one `App.cpp` file of about 6,400 lines. That is the main portfolio weakness to address.

## Approach Options

### Option A: Documentation Only

Add stronger README summaries and leave code untouched.

Pros:
- lowest risk
- fastest to review

Cons:
- does not address the large `App.cpp` criticism
- still leaves recruiters with a hard-to-scan representative code file

### Option B: Light App Split With Include Parts

Keep `App` and existing member function names, but split the implementation into clearly named `.inl` files included from `App.cpp`.

Pros:
- preserves existing private `App::Impl` access
- avoids adding a complex internal engine layer
- makes responsibilities visible in Solution Explorer and GitHub
- low compile-risk because the code remains one translation unit

Cons:
- `.inl` implementation parts are less idiomatic than true `.cpp` modules
- this is a readability refactor, not a deep architecture rewrite

### Option C: Full Private Header And Multiple `.cpp` Files

Move internal structs and `App::Impl` to a private header, then split lifecycle, rendering, UI, model loading, and utilities into normal `.cpp` files.

Pros:
- cleaner C++ compilation structure
- closer to production-style source layout

Cons:
- higher risk of accidental behavior changes
- requires exposing many private implementation details
- too heavy for the user's request to keep the code simple

## Chosen Design

Use Option B.

The implementation will make `36_AdvancedAnim_Sound_Click/App.cpp` a short index file that includes focused implementation parts:

- `App_InternalTypes.inl`: file-local structs, enums, and helper functions
- `App_Lifecycle.inl`: constructor/destructor, initialize, uninitialize, loading thread, D3D/scene/texture/imgui setup
- `App_UpdateInput.inl`: input processing and per-frame update
- `App_RenderPasses.inl`: render orchestration and render passes
- `App_ModelLoading.inl`: model load/unload and related resource setup
- `App_ImGuiPanels.inl`: ImGui control panels and debug UI
- `App_Utilities.inl`: skybox, scene image, scene switching, memory trim, HDR/swapchain helpers

The public `App.h` method names stay recognizable. The goal is not to redesign the app, but to make the existing responsibilities discoverable at a glance.

## Documentation Design

Add `Dx11/35_DeferredRendering/README.md` with the same simple style as the existing tutorial READMEs:

- what the sample demonstrates
- G-Buffer layout
- render flow
- controls/debug UI notes if present in code
- files to inspect

Update the root README near the top with a compact representative demo section:

- `36_AdvancedAnim_Sound_Click` as the main demo
- key technologies: animation blend/layer/IK/socket, deferred rendering, PBR/IBL, tone mapping, FMOD 3D sound, ImGui debug UI, multithreaded loading
- build entry point: `Dx11/TutorialApp.sln`, x64
- code map: `Dx11/Common`, `35_DeferredRendering`, `36_AdvancedAnim_Sound_Click`

Keep the existing project image table intact.

## Non-Goals

- Do not change render output, controls, resource paths, or shader behavior.
- Do not introduce a new engine framework.
- Do not rename existing public functions just to make them prettier.
- Do not convert raw D3D pointers to `ComPtr` across the demo in this pass.
- Do not restructure all 37 tutorial projects.

## Verification

Because this is a C++/Visual Studio project, verification should include:

- inspect that `App.cpp` includes all new implementation parts in a deterministic order
- check that the project file lists the new `.inl` files under headers or source-visible files
- run `git diff --check`
- run `msbuild Dx11/TutorialApp.sln /p:Configuration=Debug /p:Platform=x64` if MSBuild is available
- if MSBuild is not on PATH, report that limitation and verify the file-level structure with PowerShell checks

## Success Criteria

- `Dx11/36_AdvancedAnim_Sound_Click/App.cpp` becomes a short index-style file.
- Each extracted file has one obvious responsibility.
- Existing function names and behavior-facing structure remain intact.
- `Dx11/35_DeferredRendering/README.md` exists and explains the sample clearly.
- The root README gives recruiters a fast path to the representative demo and code structure.
