<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../16_NormalMapping/README.md) | [메인](../../README.md) | [상위](../) | [다음](../18_fbx_Animation/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/17-fbx-pmx-obj-WithPhong-info.png" width="100%" /></p>
<!-- README-INFO:END -->

## 17. fbx_pmx_obj (17_fbx_pmx_obj_WithPhong)

- 내용: FBX, PMX, OBJ 모델을 같은 조명·셰이더 경로로 렌더링하는 예제입니다. 현재 기본 장면에는 glTF/GLB 캐릭터도 사용합니다.
- 주요 구현
  - Assimp에서 임베디드 텍스처를 먼저 확인하고, 없으면 모델에 기록된 외부 텍스처 경로를 탐색합니다.
  - `scene->mMeshes[node->mMeshes[mi]]`에서 노드가 참조하는 메시를 가져옵니다.
  - 메시 정점 위치는 `mVertices`, TBN은 `mTangents`, `mBitangents`, `mNormals`에서 가져옵니다. 노드의 변환 행렬과 정점 위치는 별개의 데이터입니다.
  - 서브셋별 텍스처를 바인딩하고 셰이더로 전달합니다.

## 확인해 볼 것

같은 모델·카메라·조명에서 Phong, Blinn–Phong, Lambert, Unlit, TextureOnly를 바꿔 보세요. 실행 화면은 기본 장면의 캡처이며, 포맷별·셰이더별 비교를 기록할 때는 각 조건에서 따로 캡처해야 합니다.

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/17-fbx-pmx-obj-WithPhong.png" width="100%" /> | <img src="../../docs/media/readme/17-fbx-pmx-obj-WithPhong.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../16_NormalMapping/README.md) | [메인](../../README.md) | [상위](../) | [다음](../18_fbx_Animation/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
