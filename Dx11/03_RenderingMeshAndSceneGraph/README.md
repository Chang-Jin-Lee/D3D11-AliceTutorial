<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../02_RenderingCube/README.md) | [메인](../../README.md) | [상위](../) | [다음](../04_RenderingMeshWithTexture/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/03-RenderingMeshAndSceneGraph-info.png" width="100%" /></p>
<!-- README-INFO:END -->

## 03. RenderingMeshAndSceneGraph

<!-- README-BRAND:START -->
<p align="center"><img src="../../docs/media/branding/alice-tutorial-logo.png" width="520" alt="D3D11 Alice Tutorial mascot logo" /></p>
<!-- README-BRAND:END -->

- 내용: 부모-자식 계층(Scene Graph)으로 3개의 메쉬를 렌더링
- 주요 구현:
  - `m_CBuffers`에 3개의 상수 버퍼 데이터를 유지
  - 부모-자식 변환: `world = local * parentWorld` 적용
  - 루트와 자식1은 서로 다른 Yaw 속도로 회전, 자식2는 자식1을 중심으로 공전
  - Depth Buffer 및 DepthStencilState 활성화(Z-test)
  - ImGui로 루트/자식 상대 위치, 카메라 위치/FOV/Near/Far 실시간 조정
- 결과: 계층 변환과 깊이 테스트가 올바르게 동작하는 다중 메쉬 장면

<p align="center">
  <img src="../../docs/media/readme/03-RenderingMeshAndSceneGraph.png" width="60%" />
</p>

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/03-RenderingMeshAndSceneGraph.png" width="100%" /> | <img src="../../docs/media/readme/03-RenderingMeshAndSceneGraph.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../02_RenderingCube/README.md) | [메인](../../README.md) | [상위](../) | [다음](../04_RenderingMeshWithTexture/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
