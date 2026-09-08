<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../27_DebugDraw/README.md) | [메인](../../README.md) | [상위](../) | [다음](../29_MousePicking/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/28-Scene-Shared3DModel-Animation-info.png" width="100%" /></p>
<!-- README-INFO:END -->

## 28. Scene Shared3D Model

- 내용: 같은 모델의 메시·텍스처를 여러 인스턴스가 공유하도록 에셋 매니저를 구성한 예제입니다.
- 주요 구현
  - 모델 파일의 바이트를 해시한 데이터 기반 키로 캐시를 조회합니다.
  - 캐시는 `weak_ptr`, 사용 중인 인스턴스는 `shared_ptr`로 수명을 관리합니다.
  - 캐시가 유효하면 메시·텍스처 GPU 리소스를 재사용합니다. 트랜스폼·애니메이션 상태 같은 인스턴스별 데이터는 별도로 필요합니다.
  - 이 경로는 `FbxModel` 계열의 공유 모델을 관리합니다. 실제로는 Assimp가 읽는 GLB 캐릭터도 이 경로를 사용합니다.

## 확인해 볼 것

같은 모델을 여러 번 추가하면서 공유 메시·텍스처 수와 메모리 사용량을 살펴보세요. 리소스 공유는 중복 로딩을 줄이지만, 모든 CPU/GPU 메모리 증가를 없애는 것은 아닙니다.

씬 전환에서는 `IDXGIDevice3::Trim()`으로 드라이버가 내부의 사용하지 않는 메모리를 정리할 기회를 줍니다. 앱이 보유한 리소스 참조를 해제하는 것과는 별개이며, 즉시 VRAM 사용량이 특정 값으로 떨어지는 것을 보장하지 않습니다.

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/28-Scene-Shared3DModel-Animation.png" width="100%" /> | <img src="../../docs/media/readme/28-Scene-Shared3DModel-Animation.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../27_DebugDraw/README.md) | [메인](../../README.md) | [상위](../) | [다음](../29_MousePicking/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
