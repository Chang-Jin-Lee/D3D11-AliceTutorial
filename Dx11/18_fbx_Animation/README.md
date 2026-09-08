<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../17_fbx_pmx_obj_WithPhong/README.md) | [메인](../../README.md) | [상위](../) | [다음](../19_MultiModels/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/18-fbx-Animation-info.png" width="100%" /></p>
<!-- README-INFO:END -->

## 18. fbx Animation (18_fbx_Animation)

- 내용: 본 구조가 있는 캐릭터의 내장 애니메이션을 재생하는 예제입니다.
- 주요 구현
  - 본 계층과 부모·자식 관계를 읽고 애니메이션의 위치·회전·스케일을 평가합니다.
  - CPU에서 본 변환 팔레트를 갱신하고, 버텍스 셰이더에서 본 인덱스와 가중치로 정점을 스키닝합니다.
  - 스키닝한 정점에 월드·뷰·투영 변환을 적용합니다.
  - 본 팔레트의 용량과 정점 개수는 별개입니다. 팔레트 용량을 늘리는 것은 사용할 수 있는 본 개수를 늘리는 것이며, 정점 처리량을 직접 늘리는 설정은 아닙니다.

## 확인해 볼 것

애니메이션을 멈춘 상태와 재생 상태를 비교하고, 본의 변환이 연결된 정점에 어떻게 반영되는지 살펴보세요. 같은 포즈에서 셰이딩 모드를 바꾸면 스키닝과 조명 계산의 역할도 구분할 수 있습니다.

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/18-fbx-Animation.png" width="100%" /> | <img src="../../docs/media/readme/18-fbx-Animation.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../17_fbx_pmx_obj_WithPhong/README.md) | [메인](../../README.md) | [상위](../) | [다음](../19_MultiModels/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
