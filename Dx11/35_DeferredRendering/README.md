<!-- README-NAV-TOP:START -->
<div align="center">

[이전](../34_ToneMapping/README.md) | [메인](../../README.md) | [상위](../) | [다음](../36_AdvancedAnim_Sound_Click/README.md)

</div>
<!-- README-NAV-TOP:END -->

<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/35-DeferredRendering-info.png" width="100%" /></p>
<!-- README-INFO:END -->

# 35. Deferred Rendering



이 예제는 Forward Rendering으로 그리던 장면을 G-Buffer 기반 Deferred Rendering 구조로 확장한 단계입니다. 목적은 "많은 조명을 한 번에 다루기 위해 렌더링을 지오메트리 패스와 라이팅 패스로 나눈다"는 핵심 흐름을 직접 확인하는 것입니다.

## 핵심 구현

- Geometry Pass: 모델의 위치, 노멀, 재질 정보를 여러 렌더 타겟에 기록합니다.
- Lighting Pass: 전체화면 Quad에서 G-Buffer를 읽어 조명을 계산합니다.
- Debug View: ImGui에서 G-Buffer를 확인해 어떤 값이 저장되는지 점검합니다.
- Tone Mapping: HDR 결과를 LDR/HDR 출력에 맞게 변환합니다.

## G-Buffer 구성

이 예제는 다음 **5개의 렌더 타깃**을 사용합니다. 아래 형식은 `App::CreateGBuffer()`와 셰이더의 `SV_Target0`~`SV_Target4`에 대응합니다.

| 슬롯 | 버퍼 | DXGI 형식 | 저장 값 |
|---|---|---|---|
| 0 | PositionWS | `R16G16B16A16_FLOAT` | 월드 위치 xyz |
| 1 | NormalWS | `R16G16B16A16_FLOAT` | 월드 노멀 xyz, 음수 성분도 그대로 저장 |
| 2 | Metalness | `R8_UNORM` | 금속성 |
| 3 | Roughness | `R8_UNORM` | 거칠기 |
| 4 | BaseColor | `R8G8B8A8_UNORM_SRGB` | 베이스 컬러 |

깊이 버퍼는 별도로 사용합니다. 현재 G-Buffer 패스는 알파가 0.1보다 작은 픽셀을 버리는 cutout 방식이며, 레이스처럼 여러 반투명 층을 합성하는 경로는 없습니다. 일반적인 Deferred 확장에서는 불투명 조명 계산 뒤에 반투명 Forward 또는 OIT 패스를 추가합니다.

## 렌더 흐름

```text
PassClear
 -> PassShadow
 -> PassGBuffer
 -> PassDeferredLight
 -> PassPostProcess
 -> PassUI
```

## 주요 파일

- `App.cpp`: 렌더 패스 구성과 G-Buffer 생성
- `35_DeferredGBufferVS.hlsl`, `35_DeferredGBufferPS.hlsl`: 지오메트리 패스
- `35_DeferredLightPS.hlsl`: 라이팅 패스
- `35_DeferredShared.fxh`: G-Buffer 공유 구조
- `35_ToneMappingPS_HDR.hlsl`, `35_ToneMappingPS_LDR.hlsl`: 톤매핑

## 확인해 볼 것

- Forward와 Deferred의 패스 분리
- MRT(Multiple Render Targets) 사용
- 디버그 UI로 렌더 타겟 내용을 검증하는 흐름
- 36번 예제에서 애니메이션, 사운드, UI와 결합되기 전의 순수 Deferred Rendering 단계

<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/35-DeferredRendering.png" width="100%" /> | <img src="../../docs/media/readme/35-DeferredRendering.gif" width="100%" /> |
<!-- README-RUNTIME:END -->

<!-- README-NAV-BOTTOM:START -->
<div align="center">

[이전](../34_ToneMapping/README.md) | [메인](../../README.md) | [상위](../) | [다음](../36_AdvancedAnim_Sound_Click/README.md)

</div>
<!-- README-NAV-BOTTOM:END -->
