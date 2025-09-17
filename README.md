# D3D11-AliceTutorial
Tutorial for D3D AliceEngine

이 저장소는 [DirectX SDK Samples - Direct3D11 Tutorials](https://github.com/walbourn/directx-sdk-samples/tree/main/Direct3D11Tutorials) 을 기반으로  
학습하면서 이해하기 쉽도록 정리한 튜토리얼 프로젝트입니다.

- 환경: Windows 10 SDK, Visual Studio 2022  
- 플랫폼: Win32 Desktop (Direct3D 11.0)  
- 목적: DirectX 11 그래픽스 파이프라인의 기초 학습  

---

## 01. RenderingQuadangle
- 내용: NDC 좌표계를 기반으로 두 개의 삼각형을 그려 사각형을 렌더링  
- 주요 구현:
  - Vertex / Index Buffer 생성
  - Input Layout 및 Shader 연결
  - `IASetPrimitiveTopology` 를 이용한 삼각형 리스트 설정
- 결과: 화면 중앙에 사각형 출력

<p align="center">
  <img src="https://github.com/user-attachments/assets/a44c63b4-0313-4c7d-b98f-03bfcf7abaa0" width="60%" />
</p>

---

## 02. RenderingCube
- 내용: 단일 큐브를 렌더링
- 주요 구현:
  - 큐브 정점/인덱스 버퍼 구성 (24 정점, 36 인덱스)
  - 기본 Vertex/Pixel Shader와 Input Layout 연결
  - `DrawIndexed` 호출로 큐브 렌더링
- 결과: 화면에 큐브 1개가 렌더링됨

<p align="center">
  <img src="https://github.com/user-attachments/assets/0cde58dd-97c3-43be-abc9-021bc4bc3165" width="60%" />
</p>

---

## 03. RenderingMeshAndSceneGraph
- 내용: 부모-자식 계층(Scene Graph)으로 3개의 메쉬를 렌더링
- 주요 구현:
  - `m_CBuffers`에 3개의 상수 버퍼 데이터를 유지
  - 부모-자식 변환: `world = local * parentWorld` 적용
  - 루트와 자식1은 서로 다른 Yaw 속도로 회전, 자식2는 자식1을 중심으로 공전
  - Depth Buffer 및 DepthStencilState 활성화(Z-test)
  - ImGui로 루트/자식 상대 위치, 카메라 위치/FOV/Near/Far 실시간 조정
- 결과: 계층 변환과 깊이 테스트가 올바르게 동작하는 다중 메쉬 장면

<p align="center">
  <img src="https://github.com/user-attachments/assets/c8260ecb-9408-4313-8b99-7c4ed71c7ae3" width="60%" />
</p>

---

## 04. RenderingMeshWithTexture
- 내용: 큐브 각 면에 서로 다른 텍스처를 적용하여 렌더링
- 주요 구현:
  - `VertexPosTex`(POSITION, TEXCOORD)로 24 정점/36 인덱스 구성
  - WICTextureLoader로 PNG/JPG 텍스처 로드, 면별 SRV 바인딩 후 6회 드로우
  - 텍스처 전용 HLSL(`TexVertexShader.hlsl`, `TexPixelShader.hlsl`)과 샘플러(Linear, Wrap)
  - ImGui 컨트롤 패널 유지 + System Info(FPS(1초 갱신), GPU/CPU, RAM/VRAM) 표시
- 결과: 각 면에 다른 이미지를 가진 텍스처 큐브 렌더링

<p align="center">
  <img src="https://github.com/user-attachments/assets/f341bbb1-f09a-425c-b605-99392074e557" width="60%" />
</p>

---

## 05. Mesh (FBX)
- 내용: FBX 메시를 로드해 머티리얼별 서브셋을 렌더링
- 주요 구현:
  - Assimp로 FBX 로드, 노드 계층(Global Transform) 적용 병합
  - 머티리얼 DIFFUSE 텍스처 로드(외부 경로/임베디드 둘 다 처리, 실패 시 흰색 폴백)
  - 현재는 하얀색으로만 그림
  - `VertexPosTex`(POSITION, TEXCOORD) + 텍스처 전용 HLSL + 샘플러
  - Rasterizer Cull None, Depth 테스트 활성화
  - ImGui로 루트 위치/카메라(FOV/Near/Far) 조정
- 결과: 머티리얼/텍스처가 적용된 FBX 메시 렌더링

<p align="center">
  <img src="https://github.com/user-attachments/assets/ef96322a-786c-411d-b5f6-5e76377455da" width="60%" />
</p>


---

## 06. PMX (A-Pose)
- 내용: PMX 캐릭터를 A-포즈 실루엣(흰색)으로 단순 렌더링
- 주요 구현:
  - Assimp로 PMX 로드, 노드 계층(Global Transform) 적용 병합
  - AABB 기반 중심 이동/스케일 정규화, 카메라 자동 설정
  - 텍스처/머티리얼 생략(픽셀 셰이더 고정 마젠타색)
  - Rasterizer Cull None, Depth 테스트 활성화
  - ImGui로 루트 위치/카메라(FOV/Near/Far) 조정
- 결과: 화면에 PMX A-포즈 캐릭터 실루엣 표시
  
<p align="center">
  <img src="https://github.com/user-attachments/assets/e2de8438-8e10-4c28-a28b-ed25736a5756" width="60%" />
</p>

---

## 07. PMX Texture (07_pmxTexture)
- 내용: PMX 모델에 텍스쳐 매핑을 완성한 예제
- 주요 구현:
  - 머티리얼 DIFFUSE 텍스쳐 로딩: 외부 경로 + `Alice.fbm/` 폴더 폴백, 임베디드(`*0`) 텍스쳐 처리
  - Subset/Material 기반 드로우: 서브셋마다 SRV 바인딩 후 `DrawIndexed`
  - 샘플러/폴백: 선형 필터(Linear), Wrap, 로딩 실패 시 1x1 흰색 텍스처 폴백
  - 좌표/상태: Left-Handed, Rasterizer Cull None, Depth 테스트(LESS)
  - 셰이더: `TexVertexShader.hlsl`(POSITION/TEXCOORD), `TexPixelShader.hlsl`(텍스쳐 샘플링)
  - 카메라: RMB 드래그 회전(Yaw/Pitch), WASD 이동, Q/E 상승·하강, 마우스 휠 돌리(뷰 방향)
  - ImGui: System Info + Model Info(Vertices/Indices/Triangles/Subsets/Materials/Textures unique/fallback, Model Path, Texture Folder)
- 프로젝트: `07_pmxTexture/`
- 리소스 예시: `Resource/Nikke-Alice/`, 텍스처 폴더 `Resource/Nikke-Alice/Alice.fbm/`

<p align="center">
  <img src="https://github.com/user-attachments/assets/4ffe5d7c-6063-42f7-a9b2-7d3be574ffa0" width="60%" />
</p>

---

## 08. ImguiSystemInfo (08_ImguiSystemInfo)
- 내용: ImGui로 시스템 정보와 간단한 이미지 뷰어(파일 열기 포함)를 구현한 예제
- 주요 구현:
  - System Info: FPS(1초 갱신), GPU/CPU, RAM/VRAM 표시
  - Controls: Hanako/Yuuka 기본 이미지 크기 조절, 종횡비 고정/해제, Fit To Window/512/Fit Image, 파일 열기(탐색기)
  - 이미지 뷰어: Hanako/Yuuka 고정 슬롯 + 외부 파일 전용 슬롯(Loaded Image)
- 프로젝트: `08_ImguiSystemInfo/`
  
<p align="center">
  <img src="https://github.com/user-attachments/assets/a01532ac-eaaf-40c7-87d4-bb810bebbbfb" width="60%" />
</p>

---

## 09. Lighting (09_Lighting)
- 내용: Directional Light로 큐브를 비추는 예제
- 주요 구현:
  - Vertex: POSITION/NORMAL/COLOR, NORMAL을 VS에서 월드 노말로 변환(g_WorldInvTranspose)
  - ConstantBuffer(b0): world/view/proj/worldInvTranspose/dirLight/eyePos/pad
  - Pixel Shader: baseColor * (ambient + diffuse), pad로 디버그 모드(보라색/흰색 마커)
  - ImGui Controls: Mesh(Yaw/Pitch/Position), Camera(FOV/Near/Far), Light(Color/Direction/Position)
  - Light Marker: 라이트 위치에 작은 흰색 큐브 렌더링(스케일 0.2)

<p align="center">
  <img src="https://github.com/user-attachments/assets/f90261e6-66a4-4e38-8469-6de78fe1f791" width="60%" />
</p>

---

## 10. Static Cube SkyBox (10_StaticCube_SkyBox)
- 내용 : 정적 큐브 메시로 스카이박스를 구성해 배경으로 렌더링
- 주요 구현
  - `g_WorldViewProj = (P · V0 · Fz)^T`
  - `V0`: 뷰 행렬에서 평행이동 제거(회전만 유지)
  - `Fz`: 축 플립 행렬 `diag(1, 1, -1)` (LH 좌표계와 큐브맵 페이스 정합)
  - VS : `posH = mul(float4(posL,1), g_WorldViewProj);` → `posH.xyww`로 원근 영향 제거
  - PS : `g_TexCube.Sample(g_Sam, normalize(posL));` (큐브맵을 방향 벡터로 샘플)
  - 렌더 상태 :
  - Depth: `LESS_EQUAL`, Write OFF
  - Rasterizer: `CullFront`(내부면 렌더)
  - RS/DS 상태 저장/복구로 기존 지오메트리 영향 없음
  - 샘플러/리소스: Linear + Clamp, DDS 큐브맵 사용
  - 카메라 연동: RMB 회전 시 전체 방향에서 정상 표시, 위치는 무한원처럼 보이도록 처리(V의 평행이동 제거)
  - 참고: 큐브맵 레이아웃에 따라 `Fz`를 `Fx = diag(-1,1,1)`로 교체 가능

<p align="center">
  <img src="https://github.com/user-attachments/assets/73ea4304-b38d-4e57-a4c3-d788636263de" width="60%" />
</p>


---

## 주의사항
- 기존 레거시 DirectX SDK 종속성 제거 → Windows 10 SDK 만으로 동작  
- 수학 라이브러리: DirectXMath 사용  
- 셰이더 컴파일: D3DCompileFromFile 사용 (실험/학습 편의 목적)  
- 텍스처 로딩: DDSTextureLoader (DirectXTK / DirectXTex 기반)  

---

## 참고 자료
- [Direct3D 11 Tutorials (GitHub)](https://github.com/walbourn/directx-sdk-samples/tree/main/Direct3D11Tutorials)  
- [MSDN Direct3D 11 Programming Guide](http://msdn.microsoft.com/en-us/library/windows/apps/ff729718.aspx)  
- [DirectXMath](https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-intro)  
- [DirectXTK](https://github.com/microsoft/DirectXTK) / [DirectXTex](https://github.com/microsoft/DirectXTex)  

---

## 라이선스
본 튜토리얼 프로젝트는 학습 목적이며, 원본 샘플은 Microsoft가 제공한 [MIT License](https://github.com/walbourn/directx-sdk-samples/blob/main/LICENSE)에 따라 사용됩니다.
