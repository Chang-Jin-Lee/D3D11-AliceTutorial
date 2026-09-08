<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../19_MultiModels/README.md) | [메인](../../README.md) | [상위](../) | [다음](../21_MultiModels_With_Animations/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/20-Depth-And-Alpha-Issue-info.png" width="100%" /></p>
<!-- README-INFO:END -->

## 20. Depth Buffer and Alpha Blending Artifact

- 내용: 투명한 픽셀이 깊이를 기록해서 뒤의 물체를 가리는 현상을 의도적으로 재현하는 예제입니다.
- 주요 구현
  1. `g_Pad == 9`인 프리패스에서 투명 구간만 통과시키고 깊이를 기록합니다.
  2. `m_pColorMaskNone`으로 컬러 쓰기를 꺼서 렌더 타깃에는 색을 쓰지 않습니다.
  3. 본 패스에서는 알파 테스트로 불투명 부분을 그립니다. 앞선 프리패스가 투명 구간에도 깊이를 기록했으므로, 뒤의 물체가 깊이 테스트에서 탈락해 그 자리에 배경만 남을 수 있습니다.

## 재현 조건과 조작

`Model Loader (FBX / OBJ / PMX)` 창의 `Repro alpha occlusion (DepthWrite+ConstBlend)` 체크박스로 프리패스를 켜고 끕니다. 코드를 매번 주석 처리할 필요는 없습니다.

현재 재현 분기는 **이름이 `Tree`인 모델에만 적용**됩니다. 기본 `SampleModel.glb` 장면에서 체크박스만 켜서는 이 분기가 실행되지 않습니다. 재현을 위해서는 해당 이름의 알파 텍스처 모델과 그 뒤에서 가려질 모델이 필요합니다. 아래 실행 화면은 기본 장면이며 ON/OFF 비교 사진은 아닙니다.

## 알파 테스트와 블렌딩의 차이

- Alpha Test: `clip`으로 픽셀을 버리거나 남깁니다. 남은 불투명 픽셀은 깊이를 기록할 수 있습니다.
- Alpha Blend: 중간 알파값으로 앞뒤 색을 섞습니다. 일반적으로 불투명 물체를 먼저 그리고, 반투명 물체는 깊이 테스트 ON·깊이 쓰기 OFF 상태에서 뒤에서 앞으로 그립니다.
- 컬러 쓰기를 꺼도 깊이 쓰기는 별개의 상태이므로 계속 일어날 수 있습니다.

치마 레이스처럼 중간 알파값과 겹치는 면이 있는 모델에서는 블렌딩 순서까지 고려해야 합니다. 이 부분은 [38번](../38_StylizedToonPBR/README.md)의 구현과 [39번 OIT 제안](../../docs/project39-transparency-proposal.md)으로 이어집니다.

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/20-Depth-And-Alpha-Issue.png" width="100%" /> | <img src="../../docs/media/readme/20-Depth-And-Alpha-Issue.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../19_MultiModels/README.md) | [메인](../../README.md) | [상위](../) | [다음](../21_MultiModels_With_Animations/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
