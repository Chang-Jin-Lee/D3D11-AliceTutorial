<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../22_VMD/README.md) | [메인](../../README.md) | [상위](../) | [다음](../24_Skinned_With_Bone_Structure/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/23-Rigid-Animation-info.png" width="100%" /></p>
<!-- README-INFO:END -->

## 23. Rigid, Skinned Animation (23_Rigid_Animation)

- 내용: 메시의 본 유무와 애니메이션 유무에 따라 Static, Rigid, Skinned 경로를 구분하는 예제입니다.

| 본 | 내장 애니메이션 | 처리 |
|---|---|---|
| 없음 | 없음 | Static Mesh |
| 없음 | 있음 | 노드 변환을 재생하는 Rigid Animation |
| 있음 | 있음 | 본 가중치를 사용하는 Skinned Animation |
| 있음 | 없음 | Skinned 메시를 바인드 포즈로 표시 |

본이 있다는 사실과 재생할 클립이 있다는 사실은 별개입니다. 내장 애니메이션이 없는 GLB 캐릭터도 정적인 바인드 포즈로 표시할 수 있습니다.

Rigid 경로에서는 애니메이션 노드로 가상 본 팔레트를 만들고, 각 정점에 자신이 속한 노드의 가중치 1을 부여해 GPU 스키닝 경로를 재사용합니다. 본이 없는 모든 정적 메시를 애니메이션 메시로 바꾸는 것은 아닙니다.

## 확인해 볼 것

노드만 움직이는 모델과 관절 주변이 변형되는 스키닝 모델을 비교해 보세요. 아래 실행 화면은 한 기본 장면의 기록이며, 세 분류를 각각 촬영한 비교 이미지는 아닙니다.

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/23-Rigid-Animation.png" width="100%" /> | <img src="../../docs/media/readme/23-Rigid-Animation.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../22_VMD/README.md) | [메인](../../README.md) | [상위](../) | [다음](../24_Skinned_With_Bone_Structure/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
