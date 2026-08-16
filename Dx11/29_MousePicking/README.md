<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../28_Scene_Shared3DModel_Animation/README.md) | [메인](../../README.md) | [상위](../) | [다음](../30_PBR_BRDF/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/29-MousePicking-info.png" width="100%" /></p>
<!-- README-INFO:END -->

## 29.  mouse picking

<!-- README-BRAND:START -->
<!-- README-BRAND:END -->

- 내용 : 마우스로 오브젝트를 선택해 클릭하는 예제입니다.
- 구현방법
  - 원점, 방향을 가지는 레이를 만듭니다
  - NDC 좌표와 카메라를 사용해서 월드 공간에서 레이의 위치를 찾아냅니다
  - 그 레이를 일직선으로 진행시켰을 때 만나는 첫번째 오브젝트를 검출해냅니다
 
- 어려웠던 점
  - 구현 난이도는 쉽지만, 충돌에서의 로직과 매우 유사해서 수학식을 세우는 데 시간이 걸렸음


| 마우스로 선택한 모습 |
|---|
| <div align="center"><img src="../../docs/media/readme/29-MousePicking.png" width="600"/></div> |

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/29-MousePicking.png" width="100%" /> | <img src="../../docs/media/readme/29-MousePicking.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../28_Scene_Shared3DModel_Animation/README.md) | [메인](../../README.md) | [상위](../) | [다음](../30_PBR_BRDF/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
