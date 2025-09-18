## 07. PMX Texture (07_pmxTexture)

- 이미지를 클릭하면 이동합니다

| [유튜브](https://www.youtube.com/watch?v=JmdBz3NbKB0) | [블로그](https://velog.io/@whoamicj/DX11PMX-07pmxTexture-PMX-%ED%85%8D%EC%8A%A4%EC%B3%90-%EB%A7%A4%ED%95%91) |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/61db3fbf-0c80-4796-99eb-8e6f7454bae2" width="450"/>](https://www.youtube.com/watch?v=JmdBz3NbKB0)<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/ecd85288-7fd2-46ac-9fba-d52edfa123ee" width="230"/>](https://velog.io/@whoamicj/DX11PMX-07pmxTexture-PMX-%ED%85%8D%EC%8A%A4%EC%B3%90-%EB%A7%A4%ED%95%91)<br/></div> |


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
  <img src="https://github.com/user-attachments/assets/ecd85288-7fd2-46ac-9fba-d52edfa123ee" width="60%" />
</p>
