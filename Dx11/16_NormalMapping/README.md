<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../15_pmxWithPhong/README.md) | [메인](../../README.md) | [상위](../) | [다음](../17_fbx_pmx_obj_WithPhong/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/16-NormalMapping-info.png" width="100%" /></p>
<!-- README-INFO:END -->

# 16. NormalMapping (16_NormalMapping)

<!-- README-BRAND:START -->
<p align="center"><img src="../../docs/media/branding/alice-tutorial-logo.png" width="520" alt="D3D11 Alice Tutorial mascot logo" /></p>
<!-- README-BRAND:END -->

- 내용 : Cube의 각 면에 Normal Mapping을 하는 예제입니다.
- 주요 구현
  - TBN. 탄젠트, 비탄젠트, 노말 (Tangent, Bitangent, Normal)을 정의하고 쉐이더 코드로 GPU에 데이터를 전달합니다
  - 텍스쳐 노말맵 맵핑을 위해 Tangent를 VertexShader에서 월드 공간으로 변환하고 PixelShader에 전달합니다
  - PixelShader에서 Vertex의 Normal 대신 샘플링한 접선 공간의 normal을 월드로 변환하여 라이팅 계산에 사용합니다.

###  Normal Mapping Block

<img width="1282" height="1000" alt="스크린샷 2025-09-30 161035" src="../../docs/media/readme/16-NormalMapping.png" />

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/16-NormalMapping.png" width="100%" /> | <img src="../../docs/media/readme/16-NormalMapping.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../15_pmxWithPhong/README.md) | [메인](../../README.md) | [상위](../) | [다음](../17_fbx_pmx_obj_WithPhong/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
