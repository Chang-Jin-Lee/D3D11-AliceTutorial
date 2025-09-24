# D3D11-AliceTutorial
Tutorial for D3D AliceEngine

이 저장소는 [DirectX SDK Samples - Direct3D11 Tutorials](https://github.com/walbourn/directx-sdk-samples/tree/main/Direct3D11Tutorials) 을 기반으로  
학습하면서 이해하기 쉽도록 정리한 튜토리얼 프로젝트입니다.

- 환경: Windows 10 SDK, Visual Studio 2022  
- 플랫폼: Win32 Desktop (Direct3D 11.0)  
- 목적: DirectX 11 그래픽스 파이프라인의 기초 학습

| [Youtube](https://www.youtube.com/playlist?list=PLbPdrhrt0AJgCSKYyzjAjHwpQ_Yt4uBMx) | [Velog](https://velog.io/@whoamicj/series/DirectX11) |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/3aafc53e-d6ae-492d-8680-b240c19f1f92" width="450"/>](https://www.youtube.com/playlist?list=PLbPdrhrt0AJgCSKYyzjAjHwpQ_Yt4uBMx)<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/64a50e8e-5580-4e76-97d1-b500f9c5a8a2" width="230"/>](https://velog.io/@whoamicj/series/DirectX11)<br/></div> |

### 프로젝트 바로가기

- 이미지를 클릭하면 해당 디렉토리로 이동합니다

| 1. RenderingQuadangle | 2. RenderingCube | 3. RenderingMeshAndSceneGraph | 4. RenderingMeshWithTexture |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/a44c63b4-0313-4c7d-b98f-03bfcf7abaa0" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/01_RenderingQuadangle)<br/>NDC Quadangle</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/0cde58dd-97c3-43be-abc9-021bc4bc3165" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/02_RenderingCube)<br/>One Cube</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/c8260ecb-9408-4313-8b99-7c4ed71c7ae3" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/03_RenderingMeshAndSceneGraph)<br/>Parant-Child Transform Scene Graph</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/f341bbb1-f09a-425c-b605-99392074e557" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/04_RenderingMeshWithTexture)<br/>Texture in Cube</div> |

| 5. MeshFBX | 6. PMX A-Pose | 7. PMX Texture | 8. ImguiSystemInfo |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/ef96322a-786c-411d-b5f6-5e76377455da" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/05_Mesh)<br/>FBX,obj Mesh Loading, Rendering</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/e2de8438-8e10-4c28-a28b-ed25736a5756" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/06_pmx)<br/>PMX Data Loading, Rendering</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/4ffe5d7c-6063-42f7-a9b2-7d3be574ffa0" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/07_pmxTexture)<br/>PMX Texture Mapping</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/a01532ac-eaaf-40c7-87d4-bb810bebbbfb" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/08_ImguiSystemInfo)<br/>System Info, Image Viewer</div> |

| 9. Lighting | 10. Static Cube SkyBox | 11. Live2D | 12. Lighting Blinn Phong |
|---|---|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/f90261e6-66a4-4e38-8469-6de78fe1f791" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/09_Lighting)<br/> Lambert Light - Directional Light</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/f99e25da-ead6-4935-8ac0-ca267e0b2884" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/10_StaticCube_SkyBox)<br/>Static Cube - CubeMap SkyBox</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/3ac3d5cd-45b5-4ab1-be59-a25456c0ee9b" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/11_Live2D)<br/>D3D11 + Live2D Cubism Demo</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/bd2513e1-2cb9-4e0f-a997-bd2d1522aaa4" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/12_Lighting_BlinnPhong)<br/>Blinn Phong Demo</div> |

| 13. LineRenderer AxisDebug | 14. Lighting Phong |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/9bb70f93-463a-42bf-8cde-800651215fd4" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/13_LineRenderer_AxisDebug)<br/> LineRenderer demo</div> | <div align="center">[<img src="https://github.com/user-attachments/assets/51aff0cf-a20d-42ae-86e1-d49b701f5b88" width="200"/>](https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/tree/main/14_Lighting_Phong)<br/> Phong Mirror demo </div> | 


---

## 주의사항
- 기존 레거시인 DirectX SDK 종속성을 제거함 → Windows 10 SDK 만으로 동작  
- 수학 라이브러리인 DirectXMath 사용
- 셰이더 컴파일을 위해 D3DCompileFromFile 사용 (실험/학습 편의 목적)
- 텍스처 로딩을 위해 DDSTextureLoader 사용 (DirectXTK / DirectXTex 기반)
- Cubism SDK 사용 중

---

## 참고 자료
- [Direct3D 11 Tutorials (GitHub)](https://github.com/walbourn/directx-sdk-samples/tree/main/Direct3D11Tutorials)  
- [MSDN Direct3D 11 Programming Guide](http://msdn.microsoft.com/en-us/library/windows/apps/ff729718.aspx)  
- [DirectXMath](https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-intro)  
- [DirectXTK](https://github.com/microsoft/DirectXTK) / [DirectXTex](https://github.com/microsoft/DirectXTex)  

---

## 라이선스
본 튜토리얼 프로젝트는 학습 목적이며, 원본 샘플은 Microsoft가 제공한 [MIT License](https://github.com/walbourn/directx-sdk-samples/blob/main/LICENSE)에 따라 사용됩니다.
