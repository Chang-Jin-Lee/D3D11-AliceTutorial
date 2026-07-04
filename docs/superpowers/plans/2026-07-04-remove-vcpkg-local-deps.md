# Remove vcpkg and Add Local Dependencies Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the hard-coded vcpkg dependency and make the Direct3D 11 tutorial solution build from repo-local dependencies, while allowing existing Assimp-based model loaders to open `.gltf` and `.glb` files.

**Architecture:** Keep the existing Visual Studio solution and sample projects intact. Move runtime/build dependencies into `Dx11/third_party`, link them from `Directory.Build.Props`, compile source-only libraries through `Common.vcxproj`, and route glTF/glb files through the existing `FbxModel`/Assimp code path.

**Tech Stack:** Visual Studio 2022/2026 MSBuild, Direct3D 11, DirectXTK source files, Dear ImGui source files, stb_image for TGA fallback, Assimp SDK files generated from official Assimp source.

---

### Task 1: Vendor Third-Party Dependencies

**Files:**
- Create: `Dx11/third_party/DirectXTK`
- Create: `Dx11/third_party/imgui`
- Create: `Dx11/third_party/stb/stb_image.h`
- Create: `Dx11/third_party/assimp`

- [ ] Download DirectXTK `may2026` from the official Microsoft repository and copy only `Inc`, `Src`, and `LICENSE`.
- [ ] Download Dear ImGui `v1.92.8` from the official repository and copy only core files, DX11/Win32 backends, `misc/cpp`, and docs/license files needed to identify the package.
- [ ] Copy `stb_image.h` from the reference project or official stb source into `Dx11/third_party/stb`.
- [ ] Build or extract Assimp SDK files into `Dx11/third_party/assimp/include`, `Dx11/third_party/assimp/lib/msvc`, and `Dx11/third_party/assimp/bin/msvc`.
- [ ] Avoid directory names like `Debug`, `Release`, or `x64` under `third_party` so the current `.gitignore` does not hide required dependency files.

### Task 2: Replace vcpkg MSBuild Configuration

**Files:**
- Modify: `Dx11/Directory.Build.Props`
- Modify: `Dx11/Directory.Build.targets`
- Modify: `Dx11/Common/Common.vcxproj`
- Modify: `Dx11/Common/Common.vcxproj.filters`

- [ ] Remove `VcpkgRoot`, triplets, and vcpkg include/library directories.
- [ ] Add repo-local include paths for DirectXTK, ImGui, stb, Assimp, FMOD, and existing optional SDK folders.
- [ ] Link Assimp from `third_party/assimp/lib/msvc` and copy its DLL from `third_party/assimp/bin/msvc`.
- [ ] Add required Windows SDK libraries such as `d3d11.lib`, `dxgi.lib`, `d3dcompiler.lib`, `dxguid.lib`, `windowscodecs.lib`, `xinput.lib`, and `wbemuuid.lib`.
- [ ] Add DirectXTK and ImGui `.cpp` files to `Common.vcxproj` with precompiled headers disabled for those third-party source files.

### Task 3: Remove DirectXTex Runtime Dependency

**Files:**
- Modify: `Dx11/Common/Mesh/FbxMaterial.cpp`

- [ ] Remove `#include <DirectXTex.h>`.
- [ ] Include `stb_image.h` with `STB_IMAGE_IMPLEMENTATION` in this single translation unit.
- [ ] Replace `LoadFromTGAFile`/`CreateShaderResourceView` from DirectXTex with a small `stbi_load_from_memory` based TGA loader that creates an `R8G8B8A8_UNORM` texture.
- [ ] Keep WIC for PNG/JPG/BMP and DirectXTK DDSTextureLoader for DDS.

### Task 4: Open glTF/glb in Existing Model Loaders

**Files:**
- Modify sample `App.cpp` files that currently filter `*.fbx;*.obj;*.pmx`.

- [ ] Update file-open filters to include `*.gltf;*.glb`.
- [ ] Update extension routing so `.gltf` and `.glb` use the same `FbxModel`/Assimp path as `.fbx`.
- [ ] Preserve `.obj` and `.pmx` behavior.

### Task 5: Documentation and Verification

**Files:**
- Modify: `README.md`
- Optionally create: `Dx11/third_party/README.md`

- [ ] Document why DirectXTK, ImGui, stb_image, and Assimp are used.
- [ ] Document alternatives considered: vcpkg, NuGet, full DirectXTex, cgltf, and handwritten loaders.
- [ ] Document Assimp glTF support and the optional `imgui-node-editor` submodule requirement for `37_Blueprint`.
- [ ] Verify `rg -n "VcpkgRoot|vcpkg|DirectXTex" Dx11 --glob "!third_party/**"` returns no build dependency references.
- [ ] Build at least `Dx11/TutorialApp.sln` `Debug|x64` with MSBuild if the local toolchain is available.
