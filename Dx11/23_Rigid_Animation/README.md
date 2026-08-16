<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../22_VMD/README.md) | [메인](../../README.md) | [상위](../) | [다음](../24_Skinned_With_Bone_Structure/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/23-Rigid-Animation-info.png" width="100%" /></p>
<!-- README-INFO:END -->

## 23. Rigid, Skinned Animation (23_Rigid_Animation)

- 내용 : Rigid, Skinned Animation 분기처리를 합니다
- 주요 구현
  - 본을 읽어 개수가 0이고 애니메이션 개수도 0이면 Static Mesh입니다
  - 본의 개수 > 0, 애니메이션 개수 > 0 이면 Skinned Animation 입니다
  - 본의 개수 == 0, 애니메이션 개수 > 0 이면 Rigid Animation 입니다
  - 위의 세 개의 로직을 구현하여 분기처리를 합니다
  - 또한 본이 아예 없다면 스켈레탈 노드를 통해서 가짜 본을 만들어 줍니다

| Animation - Skinned  | Animation - Rigid  |
|---|---|
| <div align="center"><img src="../../docs/media/readme/23-Rigid-Animation.png" width="450"/></div> | <div align="center"><img src="../../docs/media/readme/23-Rigid-Animation.png" width="450"/></div> |

| Animation - Static Mesh |
|---|
| <div align="center"><img src="../../docs/media/readme/23-Rigid-Animation.png" width="600"/></div> |

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
