# 의존성 선택 기준

[프로젝트 README](../README.md)에서 분리한 문서입니다. 외부 의존성을 왜 그렇게 선택했는지와 검토한 대안을 정리합니다.

이 프로젝트는 Direct3D 11 렌더링 파이프라인 학습 레포지토리이므로, 패키지 매니저 자체를 학습/빌드의 전제 조건으로 두지 않고 필요한 라이브러리만 고정했습니다.

| 의존성 | 사용 위치 | 선택 이유 | 검토한 대안 |
|---|---|---|---|
| DirectXTK 일부 소스 | `SimpleMath`, 입력, WIC/DDS 텍스처 로딩 | Microsoft가 관리하는 D3D11 보조 라이브러리이며, 기존 코드가 이미 해당 API를 사용합니다. 전체 라이브러리 기능이 아니라 현재 필요한 소스만 `Common`에서 빌드합니다. | 직접 수학/입력/텍스처 로더 작성은 학습 가치는 있지만 교체 범위가 큽니다. NuGet/vcpkg는 사용자는 편할 수 있어도 외부 restore가 필요합니다. |
| Dear ImGui 소스 | 각 샘플의 디버그/툴 UI | 소스 포함 방식이 자연스럽고 외부 런타임 의존성이 없습니다. DX11/Win32 backend만 사용합니다. | 자체 UI 구현은 렌더링 학습보다 UI 구현 비중이 커집니다. 바이너리 패키지는 버전 추적과 재현성이 떨어집니다. |
| Assimp 로컬 DLL/lib/header | FBX, OBJ, PMX, glTF/glb 로딩 | 여러 3D 포맷을 하나의 scene/mesh/material 구조로 읽을 수 있어 포맷별 로더를 직접 유지하지 않아도 됩니다. 이 레포에서는 `FBX`, `OBJ`, `MMD/PMX`, `glTF` importer만 켠 DLL을 사용합니다. | `cgltf`는 glTF 전용이라 가볍지만 FBX/PMX 학습 범위를 잃습니다. 포맷별 직접 구현은 포트폴리오 주제에서 벗어날 만큼 작업량이 큽니다. |
| stb_image | TGA fallback | DirectXTex 전체를 런타임 의존성으로 두지 않고 TGA 한 포맷만 가볍게 처리합니다. | DirectXTex는 텍스처 변환, mipmap, 압축 등 오프라인/툴 파이프라인에 강하지만 이 레포의 런타임 TGA fallback에는 과합니다. |
| FMOD / Live2D Cubism | 사운드/Live2D 예제 | 이미 repo-local SDK 형태로 들어 있어 vcpkg 제거 대상이 아닙니다. | 사운드/Live2D 예제를 제거하면 레포 범위가 줄지만 기존 학습 주제를 잃습니다. |
