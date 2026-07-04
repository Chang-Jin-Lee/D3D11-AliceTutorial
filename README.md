# D3D11-AliceTutorial
이 저장소는 [DirectX SDK Samples - Direct3D11 Tutorials](https://github.com/walbourn/directx-sdk-samples/tree/main/Direct3D11Tutorials) 을 기반으로  
D3D 그래픽스를 학습하면서 정리한 튜토리얼 프로젝트입니다.

- 환경: Windows 11, Visual Studio 2022
- 플랫폼: Win32 Desktop (Direct3D 11.0)
- 목적: DirectX 11 그래픽스 파이프라인의 기초 학습 및 3D 기능 탐구

| [Youtube](https://www.youtube.com/playlist?list=PLbPdrhrt0AJgCSKYyzjAjHwpQ_Yt4uBMx) | [Velog](https://velog.io/@whoamicj/series/DirectX11) |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/3aafc53e-d6ae-492d-8680-b240c19f1f92" width="450"/>](https://www.youtube.com/playlist?list=PLbPdrhrt0AJgCSKYyzjAjHwpQ_Yt4uBMx)<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/64a50e8e-5580-4e76-97d1-b500f9c5a8a2" width="230"/>](https://velog.io/@whoamicj/series/DirectX11)<br/></div> |

## 대표 데모

채용자가 빠르게 볼 대표 프로젝트는 [`36_AdvancedAnim_Sound_Click`](Dx11/36_AdvancedAnim_Sound_Click)입니다. 앞 단계에서 구현한 모델 로딩, PBR/IBL, 톤매핑, 디퍼드 렌더링 위에 애니메이션 블렌딩, 레이어, IK, 소켓, FMOD 3D 사운드, ImGui 디버그 UI, 멀티스레드 로딩을 묶은 최종 데모입니다.

| 항목 | 내용 |
|---|---|
| 실행 | `Dx11/TutorialApp.sln` 열기 -> `36_AdvancedAnim_Sound_Click` 시작 프로젝트 -> `x64` 빌드 |
| 렌더링 | Forward/Deferred 전환, Shadow, PBR, IBL, Tone Mapping |
| 애니메이션 | Blend, Additive, Layer, IK, Socket |
| 사운드/UI | FMOD 3D Sound, SoundBox, ImGui Debug Panels |
| 구조 | `Dx11/Common` 공통 코드 + `35_DeferredRendering` 렌더링 단계 + `36_AdvancedAnim_Sound_Click` 통합 데모 |

### 코드 구조 요약

```text
Dx11/Common/                         공통 D3D 앱, 카메라, 메시, 애니메이션, 사운드, 로더
Dx11/35_DeferredRendering/           G-Buffer, Deferred Lighting, Tone Mapping
Dx11/36_AdvancedAnim_Sound_Click/    대표 통합 데모
  App.cpp                            구현 파일 인덱스
  App_*.inl                          수명주기, 입력/업데이트, 렌더 패스, 모델 로딩, UI, 유틸리티
```

### 프로젝트 바로가기

- 이미지를 클릭하거나, 아래 각 번호/이름을 클릭해도 해당 디렉토리로 이동합니다

| [1. RenderingQuadangle](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/01_RenderingQuadangle) | [2. RenderingCube](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/02_RenderingCube) | [3. RenderingMeshAndSceneGraph](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/03_RenderingMeshAndSceneGraph) | [4. RenderingMeshWithTexture](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/04_RenderingMeshWithTexture) |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/a44c63b4-0313-4c7d-b98f-03bfcf7abaa0" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/01_RenderingQuadangle)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/0cde58dd-97c3-43be-abc9-021bc4bc3165" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/02_RenderingCube)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/c8260ecb-9408-4313-8b99-7c4ed71c7ae3" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/03_RenderingMeshAndSceneGraph)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/f341bbb1-f09a-425c-b605-99392074e557" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/04_RenderingMeshWithTexture)</div> |

| [5. MeshFBX](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/05_Mesh) | [6. PMX A-Pose](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/06_pmx) | [7. PMX Texture](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/07_pmxTexture) | [8. ImguiSystemInfo](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/08_ImguiSystemInfo) |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/ef96322a-786c-411d-b5f6-5e76377455da" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/05_Mesh)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/e2de8438-8e10-4c28-a28b-ed25736a5756" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/06_pmx)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/4ffe5d7c-6063-42f7-a9b2-7d3be574ffa0" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/07_pmxTexture)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/a01532ac-eaaf-40c7-87d4-bb810bebbbfb" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/08_ImguiSystemInfo)</div> |

| [9. Lighting](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/09_Lighting) | [10. Static Cube SkyBox](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/10_StaticCube_SkyBox) | [11. Live2D](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/11_Live2D) | [12. Lighting Blinn Phong](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/12_Lighting_BlinnPhong) |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/f90261e6-66a4-4e38-8469-6de78fe1f791" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/09_Lighting)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/f99e25da-ead6-4935-8ac0-ca267e0b2884" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/10_StaticCube_SkyBox)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/3ac3d5cd-45b5-4ab1-be59-a25456c0ee9b" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/11_Live2D)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/bd2513e1-2cb9-4e0f-a997-bd2d1522aaa4" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/12_Lighting_BlinnPhong)</div> |

| [13. LineRenderer AxisDebug](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/13_LineRenderer_AxisDebug) | [14. Lighting Phong](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/14_Lighting_Phong) | [15. pmx With Phong](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/15_pmxWithPhong) | [16. Texture Normal Mapping](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/16_NormalMapping) |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/9bb70f93-463a-42bf-8cde-800651215fd4" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/13_LineRenderer_AxisDebug)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/51aff0cf-a20d-42ae-86e1-d49b701f5b88" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/14_Lighting_Phong)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/85fda45a-c5fc-483e-a3dc-c1be3cdc6a91" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/15_pmxWithPhong)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/b4098f54-8df7-489f-a802-f6a1e709e322" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/16_NormalMapping)</div> |

| [17. Render fbx pmx obj](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/17_fbx_pmx_obj_WithPhong) | [18. fbx Animation](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/18_fbx_Animation) | [19. MultiModels](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/19_MultiModels) | [20. Depth And Alpha Issue](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/20_Depth_And_Alpha_Issue) |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/9b00e081-40b8-43b5-8954-30c38cdf3a89" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/17_fbx_pmx_obj_WithPhong)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/ae6029d7-f61f-43ee-b6a9-482eabad4a99" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/18_fbx_Animation)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/ecdc703d-2b8e-43e2-a36d-8151e05d7347" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/19_MultiModels)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/6eb53077-2ddf-4884-95f7-1c32c6f9ed83" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/20_Depth_And_Alpha_Issue)</div> |

| [21. MultiModels With Animations](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/21_MultiModels_With_Animations) | [22. VMD Camera](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/22_VMD) | [23. Rigid, Skinned Animation](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/23_Rigid_Animation) | [24. Skinned With Bone Structure](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/24_Skinned_With_Bone_Structure) |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/33dc6967-19f2-40e0-a54a-27645860018d" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/21_MultiModels_With_Animations)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/34b73ffd-d95e-4b1b-8e64-c0ffb9de260d" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/22_VMD)</div>  | <div align="center">[<img src="https://github.com/user-attachments/assets/ceae8311-2d3e-4689-86ac-12815cddeb91" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/23_Rigid_Animation)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/c65b9a1b-4ede-419c-a8d8-260c393d2e27" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/24_Skinned_With_Bone_Structure)</div> |

| [25. ToonShading Outline](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/25_ToonShading_Outline) | [26. ShadowMap PCF](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/26_ShadowMap_PCF) | [27. debug draw box](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/27_DebugDraw) | [28. Scene Shared3DModel Animation](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/28_Scene_Shared3DModel_Animation) |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/78c3c9d0-25e5-40f2-8e32-ceddd3b10eb8" width="250"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/25_ToonShading_Outline)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/c0d19bec-bd8c-4f36-bc7f-7a85b063034d" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/26_ShadowMap_PCF)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/a200d679-ccd0-48c5-aba2-3cd265e027a2" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/27_DebugDraw)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/4d6d3ca4-8ff0-4692-8b29-b83331893bb4" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/28_Scene_Shared3DModel_Animation)</div> |

| [29. Mouse Picking](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/29_MousePicking) | [30. PBR BRDF](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/30_PBR_BRDF) | [31. IBL Image Based Lighting](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/31_IBL) | [32. Sound FMOD](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/32_Sound_FMOD) |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/d98018a9-b1a2-4dc6-bf2a-ff098796aedd" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/29_MousePicking)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/0a1d9fa5-5d8e-49be-bdd5-f36c3206f110" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/30_PBR_BRDF)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/b1ebb30c-3bd3-4804-a0d9-4c33b8d97531" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/31_IBL)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/e46606a6-984f-4bfa-876c-c9d062ca9e78" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/32_Sound_FMOD)</div> |

| [33. Sound Animation Camera Motion](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/33_Sound_Animation_Camera_Motion) | [34. Tone Mapping](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/34_ToneMapping) | [35. Deferred Rendering](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/35_DeferredRendering) | [36. Animation+ ](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/36_AdvancedAnim_Sound_Click) |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/eb54f18a-2317-4314-8f3b-5143fcce9d83" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/33_Sound_Animation_Camera_Motion)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/6959f688-1e2f-4021-b1c8-b8c7516504e8" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/34_ToneMapping)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/4c9e3b8b-0f90-4aad-8e89-39af3b373856" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/35_DeferredRendering)</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/8602f7ee-cb9f-4528-86b5-44f774a05d57" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/36_AdvancedAnim_Sound_Click)</div> |

| [37. imgui-node-editor demo](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/37_Blueprint) |
|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/5635b565-c938-4657-a149-39e80c9b5d76" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/Dx11/37_Blueprint)</div> |

---

## 빌드 방식

- 권장 환경: Windows 11, Visual Studio 2022 이상, Windows SDK, MSVC C++ workload
- 솔루션: `Dx11/TutorialApp.sln`
- 기본 플랫폼: `x64`
- vcpkg는 사용하지 않습니다. 필요한 외부 의존성은 `Dx11/third_party` 아래의 repo-local 파일을 참조합니다.
- `37_Blueprint`는 `imgui-node-editor` submodule을 사용합니다. 해당 프로젝트까지 빌드하려면 다음 명령이 필요합니다.

```bash
git submodule update --init --recursive
```

## 의존성 선택 기준

이 프로젝트는 Direct3D 11 렌더링 파이프라인 학습 레포지토리이므로, 패키지 매니저 자체를 학습/빌드의 전제 조건으로 두지 않고 필요한 라이브러리만 고정했습니다.

| 의존성 | 사용 위치 | 선택 이유 | 검토한 대안 |
|---|---|---|---|
| DirectXTK 일부 소스 | `SimpleMath`, 입력, WIC/DDS 텍스처 로딩 | Microsoft가 관리하는 D3D11 보조 라이브러리이며, 기존 코드가 이미 해당 API를 사용합니다. 전체 라이브러리 기능이 아니라 현재 필요한 소스만 `Common`에서 빌드합니다. | 직접 수학/입력/텍스처 로더 작성은 학습 가치는 있지만 교체 범위가 큽니다. NuGet/vcpkg는 사용자는 편할 수 있어도 외부 restore가 필요합니다. |
| Dear ImGui 소스 | 각 샘플의 디버그/툴 UI | 소스 포함 방식이 자연스럽고 외부 런타임 의존성이 없습니다. DX11/Win32 backend만 사용합니다. | 자체 UI 구현은 렌더링 학습보다 UI 구현 비중이 커집니다. 바이너리 패키지는 버전 추적과 재현성이 떨어집니다. |
| Assimp 로컬 DLL/lib/header | FBX, OBJ, PMX, glTF/glb 로딩 | 여러 3D 포맷을 하나의 scene/mesh/material 구조로 읽을 수 있어 포맷별 로더를 직접 유지하지 않아도 됩니다. 이 레포에서는 `FBX`, `OBJ`, `MMD/PMX`, `glTF` importer만 켠 DLL을 사용합니다. | `cgltf`는 glTF 전용이라 가볍지만 FBX/PMX 학습 범위를 잃습니다. 포맷별 직접 구현은 포트폴리오 주제에서 벗어날 만큼 작업량이 큽니다. |
| stb_image | TGA fallback | DirectXTex 전체를 런타임 의존성으로 두지 않고 TGA 한 포맷만 가볍게 처리합니다. | DirectXTex는 텍스처 변환, mipmap, 압축 등 오프라인/툴 파이프라인에 강하지만 이 레포의 런타임 TGA fallback에는 과합니다. |
| FMOD / Live2D Cubism | 사운드/Live2D 예제 | 이미 repo-local SDK 형태로 들어 있어 vcpkg 제거 대상이 아닙니다. | 사운드/Live2D 예제를 제거하면 레포 범위가 줄지만 기존 학습 주제를 잃습니다. |

## 모델 포맷

- 기존: `.fbx`, `.obj`, `.pmx`
- 추가: `.gltf`, `.glb`
- glTF/glb는 별도 로더를 새로 만든 것이 아니라, 기존 `FbxModel` 계열의 Assimp 경로로 로드합니다.
- glTF PBR 재질을 위해 base-color 텍스처는 `aiTextureType_BASE_COLOR`를 먼저 확인하고, 없을 때 기존 diffuse 경로로 fallback합니다.

## 주의사항
- 수학 라이브러리인 DirectXMath 사용
- 셰이더 컴파일을 위해 D3DCompileFromFile 사용
- 텍스처 로딩을 위해 DirectXTK WIC/DDSTextureLoader 사용
- Live2D 예제 프로젝트에서 Cubism SDK 사용

---

## 리소스파일
- IBL Sky박스
  - https://drive.google.com/file/d/1OOaj8Zh-6DOiRWh2kgyWCEB9SRHP6ZtJ/view?usp=sharing
- 캐릭터
  - https://drive.google.com/file/d/1A5OncTPxGswntuw-VTPKlqF8Gsq01n7K/view?usp=sharing

## 참고 자료
- [Direct3D 11 Tutorials (GitHub)](https://github.com/walbourn/directx-sdk-samples/tree/main/Direct3D11Tutorials)  
- [MSDN Direct3D 11 Programming Guide](http://msdn.microsoft.com/en-us/library/windows/apps/ff729718.aspx)  
- [DirectXMath](https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-intro)  
- [DirectXTK](https://github.com/microsoft/DirectXTK) / [DirectXTex](https://github.com/microsoft/DirectXTex)  

---

## 라이선스
본 튜토리얼 프로젝트는 학습 목적이며, 원본 샘플은 Microsoft가 제공한 [MIT License](https://github.com/walbourn/directx-sdk-samples/blob/main/LICENSE)에 따라 사용됩니다.
