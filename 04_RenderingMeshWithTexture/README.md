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
