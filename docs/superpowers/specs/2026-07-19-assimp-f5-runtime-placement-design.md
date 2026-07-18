# Assimp F5 Runtime Placement Design

## Context

The Assimp x64 VC143 runtime is tracked at
`Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll`. A fresh `git pull`
therefore receives the DLL correctly, but Visual Studio build output folders
remain ignored by Git. If Visual Studio considers an existing executable up to
date, pressing F5 can skip MSBuild and the existing post-build copy target does
not run. Windows then cannot find Assimp beside the executable.

The observed remote-machine state was:

- tracked third-party DLL: present;
- `Dx11/bin` runtime DLL: absent;
- `Dx11/x64/Debug` runtime DLL: absent.

Assimp is not a Git submodule. The only repository submodule is
`Dx11/third_party/imgui-node-editor`.

## Goal

After pulling `main`, a developer can open `Dx11/TutorialApp.sln`, select an
x64 Debug or x64 Release sample, and press F5 without manually copying Assimp or
forcing a rebuild. The common `Dx11/bin` executable collection must also have
the runtime available.

## Non-goals

- Adding a 32-bit Assimp runtime for Win32 configurations.
- Rebuilding Assimp or changing the samples to static Assimp linkage.
- Changing the existing project output layout.
- Removing the existing post-build runtime copy target.

## Chosen design

Keep the canonical tracked DLL under `third_party`, and additionally track the
same x64 binary at each executable location used by the repository:

- `Dx11/bin/assimp-vc143-mt.dll`;
- `Dx11/x64/Debug/assimp-vc143-mt.dll`;
- `Dx11/x64/Release/assimp-vc143-mt.dll`.

Add narrow `.gitignore` exceptions for only these three files. All other build
outputs under `bin`, `x64/Debug`, and `x64/Release` remain ignored. Git stores
identical file contents as one blob even though the checkout materializes the
DLL in multiple directories.

Retain `CopyThirdPartyRuntimeDlls` in `Dx11/Directory.Build.targets`. A real
build will continue refreshing `$(TargetDir)` and the common bin directory from
the canonical third-party copy. Directly tracked output copies cover the case
where Visual Studio skips the build before launching an already-built sample.

## Validation

Extend `tools/tests/test_portable_runtime.ps1` so it verifies all four Assimp
paths:

1. each DLL exists and is non-empty;
2. each DLL is tracked by Git and is not ignored;
3. every output copy has the same SHA-256 hash as the canonical third-party DLL;
4. the existing MSBuild copy target still names the canonical runtime and copies
   it to both `$(TargetDir)` and `$(CommonBinDir)`.

The regression test must fail before the output copies and ignore exceptions are
added, then pass after the minimal change.

## Risks and controls

- A future Assimp update could leave stale output copies. Hash verification
  fails immediately if any tracked copy differs from the canonical DLL.
- Build folders become partially tracked. Narrow ignore exceptions expose only
  `assimp-vc143-mt.dll`; executables, PDBs, intermediate files, and all other
  generated artifacts remain ignored.
- The bundled runtime is x64-only. The design deliberately covers x64 Debug and
  x64 Release, matching the existing Assimp binary.
