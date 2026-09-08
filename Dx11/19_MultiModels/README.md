<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../18_fbx_Animation/README.md) | [메인](../../README.md) | [상위](../) | [다음](../20_Depth_And_Alpha_Issue/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/19-MultiModels-info.png" width="100%" /></p>
<!-- README-INFO:END -->

## 19. fbx, obj, pmx MultiModels (19_MultiModels)

- 내용: 여러 모델을 동시에 렌더링하는 예제입니다.
- 주요 구현
  - 17번의 모델 렌더링 로직을 모델별 `ModelEntry`로 묶습니다.
  - 모델의 버퍼·재질·트랜스폼을 벡터로 관리하고 순회하며 그립니다.

## 확인해 볼 것

모델 하나의 위치·회전·스케일을 바꿨을 때 다른 모델이 영향을 받지 않는지 확인합니다. 여러 모델에 같은 셰이더를 사용해도 드로우마다 각 모델의 상수 버퍼와 텍스처를 바인딩해야 합니다.

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/19-MultiModels.png" width="100%" /> | <img src="../../docs/media/readme/19-MultiModels.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../18_fbx_Animation/README.md) | [메인](../../README.md) | [상위](../) | [다음](../20_Depth_And_Alpha_Issue/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
