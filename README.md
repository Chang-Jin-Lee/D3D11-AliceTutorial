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
