# D3D11-AliceTutorial
Tutorial for D3D AliceEngine

?�� ????��?��?�� [DirectX SDK Samples - Direct3D11 Tutorials](https://github.com/walbourn/directx-sdk-samples/tree/main/Direct3D11Tutorials) ?�� 기반?���?  
?��?��?��면서 ?��?��?���? ?��?���? ?��리한 ?��?��리얼 ?��로젝?��?��?��?��.

- ?���?: Windows 10 SDK, Visual Studio 2022  
- ?��?��?��: Win32 Desktop (Direct3D 11.0)  
- 목적: DirectX 11 그래?��?�� ?��?��?��?��?��?�� 기초 ?��?��  

---

## 01. RenderingQuadangle
- ?��?��: NDC 좌표계�?? 기반?���? ?�� 개의 ?��각형?�� 그려 ?��각형?�� ?��?���?  
- 주요 구현:
  - Vertex / Index Buffer ?��?��
  - Input Layout �? Shader ?���?
  - `IASetPrimitiveTopology` �? ?��?��?�� ?��각형 리스?�� ?��?��
- 결과: �? ?���? ?��?�� 기�???���? ?���? 중앙?�� ?��각형 출력

<p align="center">
  <img src="https://github.com/user-attachments/assets/a44c63b4-0313-4c7d-b98f-03bfcf7abaa0" width="40%" />
</p>

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
  <img src="https://github.com/user-attachments/assets/ef96322a-786c-411d-b5f6-5e76377455da" width="50%" />
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
  <img src="https://github.com/user-attachments/assets/e2de8438-8e10-4c28-a28b-ed25736a5756" width="50%" />
</p>

---

## 주의?��?��
- 기존 ?��거시 DirectX SDK 종속?�� ?���? ?�� Windows 10 SDK 만으�? ?��?��  
- ?��?�� ?��?��브러�?: DirectXMath ?��?��  
- ?��?��?�� 컴파?��: D3DCompileFromFile ?��?�� (?��?��/?��?�� ?��?�� 목적)  
- ?��?���? 로딩: DDSTextureLoader (DirectXTK / DirectXTex 기반)  

---

## 참고 ?���?
- [Direct3D 11 Tutorials (GitHub)](https://github.com/walbourn/directx-sdk-samples/tree/main/Direct3D11Tutorials)  
- [MSDN Direct3D 11 Programming Guide](http://msdn.microsoft.com/en-us/library/windows/apps/ff729718.aspx)  
- [DirectXMath](https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-intro)  
- [DirectXTK](https://github.com/microsoft/DirectXTK) / [DirectXTex](https://github.com/microsoft/DirectXTex)  

---

## ?��?��?��?��
�? ?��?��리얼 ?��로젝?��?�� ?��?�� 목적?���?, ?���? ?��?��??? Microsoft�? ?��공한 [MIT License](https://github.com/walbourn/directx-sdk-samples/blob/main/LICENSE)?�� ?��?�� ?��?��?��?��?��.
