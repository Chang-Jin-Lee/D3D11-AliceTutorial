<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../35_DeferredRendering/README.md) | [메인](../../README.md) | [상위](../) | [다음](../37_Blueprint/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/36-AdvancedAnim-Sound-Click-info.png" width="100%" /></p>
<!-- README-INFO:END -->

# 36. Animation, FMOD 3D Sound, Multithread

이 이후의 애니메이션 구현은 다음의 레포에서 계속됩니다.

https://github.com/Chang-Jin-Lee/D3D11-AliceAnimation

## 애니메이션

- Animation Blend
- Additive
- Animation Layer
- IK
- Socket

현재 포트폴리오 화면은 공개 `SampleModel.glb` 네 인스턴스가 서로 다른 클립을 재생하며, 12초마다 다음 클립 조합으로 크로스페이드합니다. 각 조합의 4~7초 구간에는 0번 캐릭터의 상체 레이어를 적용합니다. CCD IK는 이 결정론적 쇼케이스 타임라인에는 포함하지 않습니다.

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/36-AdvancedAnim-Sound-Click.png" width="420"/> | <img src="../../docs/media/readme/36-advanced-anim-sound-click.gif" width="420"/> |

## 구현 내용

- 공개 배포 가능한 VRoid 캐릭터 모델을 사용합니다.
- 외부 FBX 애니메이션 클립을 glTF/glb 캐릭터의 스켈레톤 이름에 매칭합니다.
- Unreal 기준 FBX 위치 키를 glTF/glb 기준 미터/Y-up 좌표계로 변환해 루트/골반 트랜스폼이 튀지 않게 보정합니다.
- FMOD 3D 사운드, ImGui 디버그 패널, 멀티스레드 리소스 로딩을 함께 확인할 수 있습니다.
- 일반 실행에서는 기존 `Controls`, `ShadowMap`, `Scene Collection`, `Console`, 사운드·시스템 디버그 패널을 표시합니다. README 캡처 모드에서는 패널 대신 클립 조합과 현재 기법을 설명하는 간결한 HUD만 표시합니다.
- Advanced Rig은 네 캐릭터 팔레트를 포트폴리오 타임라인이 소유하므로 비활성 상태와 그 이유를 패널에 명시합니다.
- Deferred Rendering은 이 포트폴리오 구도의 조명·합성을 보존하기 위해 Forward로 고정하며, 일반 실행의 Controls와 Deferred 패널에 비활성 사유를 표시합니다.
- 공개 glTF 캐릭터의 미터 단위를 이 장면의 월드 구성에 맞춰 명시적으로 `80` 스케일로 변환합니다. 네 캐릭터를 담기 위해 카메라가 약 285 월드 단위로 멀어진 만큼 기본 이동 속도는 `40`이며, `Controls`에서 다시 조절할 수 있습니다.

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/36-AdvancedAnim-Sound-Click.png" width="100%" /> | <img src="../../docs/media/readme/36-advanced-anim-sound-click.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../35_DeferredRendering/README.md) | [메인](../../README.md) | [상위](../) | [다음](../37_Blueprint/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
