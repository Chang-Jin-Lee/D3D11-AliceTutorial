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
- 결과: 창 왼쪽 아래 기준으로 화면 중앙에 사각형 출력

<p align="center">
  <img src="https://github.com/user-attachments/assets/a44c63b4-0313-4c7d-b98f-03bfcf7abaa0" width="40%" />
</p>

## 02. RenderingCube
- 내용: 단일 큐브를 렌더링
- 주요 구현:
  - 큐브 정점/인덱스 버퍼 구성 (24 정점, 36 인덱스)
  - 기본 Vertex/Pixel Shader와 Input Layout 연결
  - `DrawIndexed` 호출로 큐브 렌더링
- 결과: 화면에 큐브 1개가 렌더링됨

<p align="center">
  <img src="https://github.com/user-attachments/assets/0cde58dd-97c3-43be-abc9-021bc4bc3165" width="40%" />
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
  <img src="https://github.com/user-attachments/assets/c8260ecb-9408-4313-8b99-7c4ed71c7ae3" width="40%" />
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
  <img src="https://github.com/user-attachments/assets/f341bbb1-f09a-425c-b605-99392074e557" width="40%" />
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
