# Assimp Runtime Distribution and Live2D Sample Design

## Goal

Make a fresh clone of the tutorial repository contain the Assimp runtime required by the Assimp-based samples, and make `11_Live2D` open with a redistributable example model already loaded. The user will perform the final runtime test on another computer.

## Root Cause

`Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll` exists in the current checkout, and `Dx11/Directory.Build.targets` already copies it to both the project output directory and `Dx11/bin`. However, the repository-wide `[Bb]in/` rule in `.gitignore` ignores the Assimp `bin` directory. The DLL is therefore absent from Git even though the Assimp import library and headers are tracked.

On a fresh clone, the `Exists(...)` condition in `CopyThirdPartyRuntimeDlls` evaluates to false. The build can link through the tracked import library, but executables that import Assimp cannot start because Windows cannot find `assimp-vc143-mt.dll`.

## Considered Approaches

### 1. Track the existing DLL and retain the copy target — selected

Add narrow `.gitignore` exceptions for the Assimp runtime directory and DLL, then commit the existing x64 VC143 DLL. Keep `Directory.Build.targets` as the single shared deployment path for every executable project.

This is the smallest change, preserves the current Assimp build and importer set, works offline after cloning, and repairs all affected samples through one shared rule.

### 2. Rebuild and link Assimp statically

Replace the import library with a static Assimp build and update compile definitions and runtime-library compatibility. This removes the runtime DLL but changes the third-party binary contract and has substantially higher compatibility and validation risk.

### 3. Download Assimp during setup or build

Add a bootstrap script or build target that downloads the DLL. This keeps the binary out of Git, but makes fresh builds network-dependent and introduces version, integrity, and availability concerns.

## Assimp Runtime Design

- Add explicit `.gitignore` exceptions only for `Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll` and its parent directories.
- Commit the existing DLL alongside the already tracked `assimp-vc143-mt.lib` and headers.
- Keep the current `CopyThirdPartyRuntimeDlls` target, which copies the DLL to `$(TargetDir)` and `$(SolutionDir)bin` for all non-static-library projects.
- Add a dependency verifier that fails if the DLL is missing, ignored, untracked, or no longer referenced by the shared copy target.
- Do not duplicate the DLL in individual tutorial project directories.

This single shared fix covers `05_Mesh`, `06_pmx`, `07_pmxTexture`, Assimp consumers from `17_fbx_pmx_obj_WithPhong` through `36_AdvancedAnim_Sound_Click`, and any other executable that acquires an Assimp dependency through the shared model library.

## Live2D Sample Asset

Use the `Skeleton_Model` runtime bundle from BluePengcho's `Open_Source_Hand_Tracking_Live2D_Model` repository. Its README explicitly permits the model and its parts to be freely used, copied, and edited without attribution. The source is:

`https://github.com/BluePengcho/Open_Source_Hand_Tracking_Live2D_Model`

Store only the files needed by the current Cubism loader under:

```text
Dx11/Resource/Live2D/Skeleton_Model/
  Skeleton_Model.model3.json
  Skeleton_Model.moc3
  Skeleton_Model.cdi3.json
  Skeleton_Model.2048/texture_00.png
  README.md
```

The local README records the upstream repository, pinned source commit, upstream permission statement, and the list of copied files. Official Live2D character samples are not used because their Free Material License has additional contractual and redistribution conditions.

## Live2D Loading Flow

Refactor the existing file-dialog load sequence into one private `App::LoadLive2DModel(const std::wstring&)` helper. The helper will:

1. release the previous model and cached UI texture references;
2. construct `MinimalUserModel`;
3. load the selected `model3.json` and bind its textures;
4. cache motion groups;
5. configure the current high-precision-mask and clipping-buffer settings;
6. set a success or failure status without terminating the application.

After Cubism Framework and its D3D11 renderer have initialized, `OnInitialize` calls the helper with:

```text
..\Resource\Live2D\Skeleton_Model\Skeleton_Model.model3.json
```

This relative path matches both supported layouts:

- `Dx11/bin` as the working directory, resolving to the tracked `Dx11/Resource` tree;
- the Visual Studio project output directory, where the existing pre-build resource copy creates the sibling `Resource` directory.

The existing **Open model3.json** button remains available and calls the same helper, so users can replace the bundled model interactively without duplicated loading logic.

## Error Handling

- A missing or invalid default Live2D model does not abort D3D or Cubism initialization. The UI reports the failing path and load status, and the file picker remains usable.
- The shared Assimp deployment verifier reports the exact missing, ignored, or untracked condition before a commit can be considered complete.
- No network access is required during build or runtime.

## Documentation

- Update `Dx11/11_Live2D/README.md` to state that the bundled open-source model loads automatically, identify its path and origin, and retain manual model-selection instructions.
- Update `Dx11/third_party/README.md` to state that the Assimp DLL is tracked and copied by shared MSBuild targets.

## Verification

The implementation will use dependency-free static checks rather than launching the samples, as requested:

- first run the new verifier in a failing state before the `.gitignore`/tracking fix;
- verify the Assimp DLL exists, is not ignored, is tracked by Git, and is named in `Directory.Build.targets`;
- verify the Live2D `model3.json` references files that exist in the bundled asset directory;
- verify `11_Live2D` names the bundled `model3.json` as its startup model and uses the shared helper for both automatic and manual loading;
- run `git diff --check` and inspect the staged file list;
- do not launch tutorial executables or claim cross-computer runtime success.

## Git Delivery

Commit the implementation on the current `main` branch and push `main` to `origin`. The push will also publish the three pre-existing local commits by which `main` was already ahead of `origin/main` when this work began.
