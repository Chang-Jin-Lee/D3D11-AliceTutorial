# third_party

This folder contains repo-local dependencies used to keep the tutorial solution independent from vcpkg.

| Package | Version / source | Notes |
|---|---|---|
| DirectXTK | `may2026`, Microsoft DirectXTK | Only headers plus the source files currently used by the samples are included. |
| Dear ImGui | `v1.92.8`, ocornut/imgui | Core files, Win32/DX11 backends, and `misc/cpp/imgui_stdlib` are included. |
| Assimp | `v6.0.5`, assimp/assimp | Built as a Windows x64 MSVC DLL with only `OBJ`, `FBX`, `GLTF`, and `MMD` importers enabled. |
| stb_image | local copy from `dx11-math-shader` reference project | Used only as a lightweight TGA fallback in `Common/Mesh/FbxMaterial.cpp`. |
| FMOD | Existing SDK files | Used by sound samples. |
| Cubism SDK | Existing SDK files | Used by the Live2D sample. |

`imgui-node-editor` is a git submodule used by `37_Blueprint`.
Run `git submodule update --init --recursive` before building that project.
