# 39. Transparency OIT 설계

39번 제안 문서와 사용자의 시작 요청을 기준으로 구현한다. 기존 수정은 `b3d4ba5`로 `main`에 푸시했으며, 새 예제는 `codex/project39-transparency-oit`에서 작업한다.

## 학습 목표와 범위

같은 캐릭터·포즈·조명에서 Alpha Test, Sorted Alpha Blend, Weighted Blended OIT를 비교한다. 직접 만든 VRM에서 내보낸 `SampleModel.glb`의 레이스와 서로 교차하는 색 사각형 3장을 사용한다. 불투명 가림판으로 깊이 테스트와 깊이 쓰기를 구분한다. OIT는 순서 독립적인 **근사 합성**이며 정확한 per-pixel sorted OVER의 대체 정답으로 설명하지 않는다.

기존 `Common`, `Common/Animation`, 모델·텍스처와 1–38번 렌더링 코드는 변경하지 않는다. 38번의 반투명 깊이/노멀 기록과 외곽선·그림자는 가져오지 않는다. 심볼릭 링크나 junction을 만들지 않는다. 새 엔진 계층이나 외부 라이브러리를 추가하지 않는다.

## 화면과 재질

- 기본 모드는 Weighted Blended OIT. 캐릭터와 교차 면을 함께 보여 주되, 각각 단독으로 보는 장면 선택과 레이스 확대 구도를 제공한다.
- glTF OPAQUE/MASK/BLEND와 alphaCutoff, baseColorFactor를 보존한다. 레이스 실험 토글은 인덱스 4와 이름 `N00_002_01_Tops_01_CLOTH_02 (Instance)`가 모두 일치한 재질에만 MASK→BLEND 오버라이드를 적용한다. 기본 토글은 꺼져 있으며, 비교 캡처에서 명시적으로 켠다.
- Alpha Test 비교 모드는 BLEND로 분류된 재질을 실험 cutoff로 잘라 불투명 패스에 그린다. 원래 MASK는 원래 cutoff를 유지한다. 알파 배율은 실험 대상 레이스와 합성 사각형에만 적용한다.
- Sorted 모드는 모든 투명 draw의 월드 중심을 뷰 공간으로 변환한 깊이로 안정적인 back-to-front 정렬을 한다. 스키닝 캐릭터는 고정 포즈를 사용하며 정렬 중심도 같은 포즈에서 구한다. OIT에는 정렬이 필요 없다.
- `1/2/3` 모드, `L` 레이스 실험, `R` 실제 투명 draw 순서 역전, `O` 카메라 회전, `C` 레이스 확대/전체, `S` 장면 전환. ImGui에 같은 컨트롤, 알파·cutoff·배경색·노출, Accumulation/Revealage 보기, CPU 정렬 시간과 GPU 투명/합성 시간을 둔다. 역전은 Sorted의 정렬 결과도 뒤집는 의도적인 실패 실험으로 표시한다.
- 모델과 핵심 셰이더 오류는 초기화 실패로 알린다. GPU 타이밍만 선택 기능이며 warming up/unavailable을 표시한다. 최소화는 렌더링을 건너뛰고 창 크기 변경은 타깃을 재생성한다.

## 렌더링 계약

`OitPipeline`은 Win32/ImGui/Assimp에 의존하지 않고 렌더 타깃, 깊이/블렌드 상태, fullscreen 합성과 GPU 쿼리만 소유한다. `App`은 모델·사각형, 카메라·고정 포즈, 표면 셰이더, draw 목록과 UI를 소유한다. 이 경계로 실제 앱의 OIT 코드를 WARP 테스트에 직접 링크한다.

1. Opaque/MASK → linear HDR `R16G16B16A16_FLOAT`와 D32 깊이. 깊이 테스트/쓰기 ON.
2. Sorted → 같은 HDR, straight-alpha OVER, 깊이 테스트 ON/쓰기 OFF. Alpha Test → HDR, 블렌딩 OFF/깊이 쓰기 ON.
3. OIT → Accumulation `R16G16B16A16_FLOAT` clear 0, Revealage `R16_FLOAT` clear 1. 독립 MRT 블렌드: Accum은 ONE+ONE, Reveal은 ZERO+INV_SRC_COLOR. 깊이 테스트 ON/쓰기 OFF.
4. Resolve → 별도 linear HDR. `weightedColor = accum.rgb / accum.a`, `coverage = 1 - reveal`; 색은 `weightedColor * coverage + opaque * reveal`. 빈 픽셀은 opaque 그대로. 0 나눗셈과 비유한 값을 방어한다.
5. Present → 선형 HDR 합성 후 노출·tone mapping·sRGB 변환을 정확히 한 번 적용한다. UI는 그 뒤에 그린다.

공유 `39_Transparency.fxh`는 OIT 출력과 가중치를 정의한다. 깊이는 `saturate((viewDepth - nearPlane) / (farPlane - nearPlane))`인 정규화 선형 뷰 깊이이다. 가중치는 `clamp(0.01 + 8 * pow(1 - depth, 3), 0.01, 8)`이며 0 알파는 discard한다. 기여 HDR 색을 [0, 8]로 제한하여 이 소규모 장면에서 FP16 여유를 확보하고, 합성 시 비유한 값도 방어한다. 임의 개수의 층에 대해 FP16 overflow가 절대 없다고 주장하지 않는다.

### OitPipeline 공개 인터페이스

namespace `Transparency39`:

```cpp
enum class Mode { AlphaTest, SortedBlend, WeightedOit };
enum class DebugView { Composite, Accumulation, Revealage };
struct GpuTimings { bool available; bool valid; double transparentMs; double compositeMs; };
class OitPipeline {
public:
    bool Initialize(ID3D11Device* device, const std::wstring& shaderDirectory);
    bool Resize(ID3D11Device* device, UINT width, UINT height);
    void BeginOpaque(ID3D11DeviceContext* context, const float clearColor[4]);
    void BeginTransparency(ID3D11DeviceContext* context, Mode mode);
    void EndTransparency(ID3D11DeviceContext* context);
    void Resolve(ID3D11DeviceContext* context);
    void Present(ID3D11DeviceContext* context, ID3D11RenderTargetView* output,
                 float exposure, DebugView debugView);
    ID3D11Texture2D* LinearTexture() const;
    ID3D11Texture2D* DepthTexture() const;
    ID3D11ShaderResourceView* AccumulationSRV() const;
    ID3D11ShaderResourceView* RevealageSRV() const;
    const GpuTimings& Timings() const;
};
```

BeginOpaque/BeginTransparency는 OM과 viewport를 설정한다. App/test는 이어서 자신의 IA·VS·PS·RS·CB·texture/sampler를 설정한다. Resolve/Present는 fullscreen draw의 IA·VS·PS·RS·depth/blend와 SRV를 설정하고 마지막에 SRV를 해제한다. Resolve는 매 프레임 호출한다. 출력 RTV는 내부 텍스처와 다른 리소스여야 한다. Initialize/Resize는 실패를 반환한다. Resize 전에 호출자가 이전 OM/SRV 바인딩을 해제한다.

## 검증과 공개 자료

WARP 픽셀 테스트는 실제 OitPipeline과 공유 HLSL을 사용해 빈 투명 패스, 단층의 해석적 결과, 다른 색 두 층의 순서 역전과 Revealage, opaque에 의한 가림, alpha 0의 색·깊이 무변경, 리사이즈, Accumulation/Revealage 디버그 표시를 검증한다. Sorted 순서를 뒤집으면 색이 바뀌는 대조 실험도 포함한다. FP16 오차를 허용하며 소스 문자열 정규식 검사로 대체하지 않는다.

VS 솔루션, 아이콘 대상, README 미디어 manifest와 생성 내비게이션에 39번을 추가한다. README는 한국어의 짧은 학습 기록 말투를 유지하며 구현·조작·합성식·깊이 공간·근사의 한계·실측 항목을 설명한다. PNG와 GIF, 레이스 비교는 실행 화면에서 만들며 서로 다른 모드를 실제로 전환해 촬영한다.

참고 원문: [McGuire/Bavoil, 2013](https://jcgt.org/published/0002/02/09/), [D3D11 independent blending](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_blend_desc), [glTF alpha coverage](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#alpha-coverage).
