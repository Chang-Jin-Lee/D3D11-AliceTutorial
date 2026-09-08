<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../30_PBR_BRDF/README.md) | [메인](../../README.md) | [상위](../) | [다음](../32_Sound_FMOD/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/31-IBL-info.png" width="100%" /></p>
<!-- README-INFO:END -->

# 31. IBL (Image Based Lighting)

## IBL을 왜 쓰는가

단색 ambient만 더하면 주변의 어느 방향에서 빛이 오는지 표현하기 어렵습니다. IBL은 환경 맵에서 조명 정보를 가져와 피부·천·금속에 주변 색과 반사가 어떻게 나타나는지 살펴보는 방법입니다. 이 예제는 미리 준비된 diffuse/specular 큐브맵과 BRDF LUT를 사용합니다.

## 주요 구현

- Diffuse IBL: 미리 적분된 diffuse 환경 맵을 법선 N 방향으로 샘플링합니다. PBR의 `kD = (1 - F) * (1 - metalness)`로 금속의 난반사를 줄입니다.
- Specular IBL: `reflect(-V, N)` 방향으로 거칠기별 specular 큐브맵을 샘플링하고, `N·V`와 roughness로 BRDF LUT를 조회합니다.
- AO: 구석이나 접촉부에 들어오는 환경광을 줄이는 근사값입니다.

## 확인해 볼 것

같은 카메라·조명에서 metalness를 0과 1로 바꾸고, roughness를 낮은 값에서 높은 값으로 올려 보세요. 반사의 색, 선명도, 퍼지는 정도가 어떻게 달라지는지 확인합니다.

아래 스크린샷과 GIF는 기본 장면입니다. 재질값별 비교를 기록할 때는 값을 바꾼 뒤 각각 캡처해야 합니다. IBL 리소스가 없으면 시작 시 검증·설치를 시도하며, 오프라인에서는 직접광과 중립 배경으로 실행됩니다.

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/31-IBL.png" width="100%" /> | <img src="../../docs/media/readme/31-IBL.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../30_PBR_BRDF/README.md) | [메인](../../README.md) | [상위](../) | [다음](../32_Sound_FMOD/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
