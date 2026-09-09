# Project 40 MSAA & Alpha-to-Coverage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 같은 캐릭터 레이스와 진단 장면에서 Alpha Test 1x / Alpha Test 4x / A2C 4x를 비교하는 실행 가능한 DX11 예제와 검증·실행 자료를 만든다.

**Architecture:** `CoverageScene`은 재질 분류와 정렬, `MsaaPipeline`은 색상·깊이 타깃과 렌더 상태·Resolve·출력·측정을 담당한다. `App`이 기존 로더와 고정 포즈를 연결하며, 독립 테스트는 앱과 같은 C++ 구현과 공유 HLSL을 실행한다. 의존 순서는 Task 1 → 2 → 3 → 4 → 5다.

**Tech Stack:** Visual Studio 2022 v143, C++20, Direct3D 11 / Shader Model 5, 기존 Common·Assimp·DirectXTK·ImGui, PowerShell 7.

**Spec:** [승인된 설계](../specs/2026-09-09-project40-msaa-alpha-to-coverage-design.md). 설계와 이 계획을 모두 읽고 실행한다.

## Global Constraints

- 프로젝트: `Dx11/40_MSAA_AlphaToCoverage`
- 작업 브랜치: `codex/project40-msaa-alpha-to-coverage`
- 빌드: Visual Studio 2022, Direct3D 11, Debug/Release x64
- namespace는 `Coverage40`으로 분리한다.
- 기존 `Common`, `Common/Animation`, 1–39번 렌더링 코드, 모델·텍스처 파일은 변경하지 않는다. 기존 로더와 애니메이터는 그대로 사용한다. 새 라이브러리, 공통 엔진 계층, 심볼릭 링크, junction은 추가하지 않는다. 프로젝트 등록과 README·미디어 도구의 40번 연결은 변경 범위에 포함한다.
- 2x·8x 선택, 화면 분할, 샘플별 확대 뷰, TAA·FXAA, 샘플 주파수 셰이딩, 알파 커버리지 보정 mipmap은 이번에 추가하지 않는다.
- 모든 모드에서 원래 BLEND는 정렬 블렌딩을 유지한다. 비교용 레이스와 진단 패턴만 처리 방식을 바꾼다.
- 지원되지 않은 4x를 1x로 대체하고 4x라고 표시하지 않는다. HDR Resolve 후 화면 변환을 한 번만 적용한다.
- 실제 코드·공유 HLSL을 테스트한다. 각 기능은 관련 실패 테스트를 먼저 기록하고 구현 후 같은 명령으로 통과를 확인한다.
- 코드·문서 편집은 `apply_patch`를 사용한다. 기존 미추적 `.superpowers/`와 다른 worktree는 보존한다. 실행 환경의 작업 격리는 실행 시 `using-git-worktrees` 절차로 확인한다. 이 계획 작성 단계에서는 worktree를 추가하지 않는다.
- 커밋은 각 작업의 검증·리뷰 후 해당 파일만 지정한다. 위임 실행을 선택하면 작업자는 git을 변경하지 않고 주 에이전트가 커밋한다. 동시에 Common을 빌드하지 않는다.
- 구현·검증이 끝나면 PR 초안을 Markdown으로 먼저 준비하며, PR 생성과 머지는 별도 요청 없이 진행하지 않는다. 자동 푸시도 이 계획에 포함하지 않는다.

## 파일 구성

| 작업 | 생성·수정 파일 | 책임 |
|---|---|---|
| 1 | `Dx11/40_MSAA_AlphaToCoverage/CoverageScene.{h,cpp}` | 모드·재질 규칙, 지원 판단, 메모리 산식, 정렬 |
| 1 | `tools/tests/native/CoverageSceneTests.{cpp,vcxproj}`, `tools/tests/test_coverage_scene.ps1` | 독립 C++ 동작 검사 |
| 2–3 | `Dx11/40_MSAA_AlphaToCoverage/MsaaPipeline.{h,cpp}` | 타깃·OM 상태·Resolve·Present, 이후 타이밍 추가 |
| 2 | `Dx11/40_MSAA_AlphaToCoverage/40_AlphaCoverage.fxh`, `40_Present.hlsl` | 공유 알파 규칙과 출력 변환 |
| 2–3 | `tools/tests/native/MsaaPipelineTests.{cpp,vcxproj}`, `MsaaTestSurface.hlsl`, `tools/tests/test_msaa_pipeline.ps1` | WARP 렌더링·측정 검사 |
| 4 | `Dx11/40_MSAA_AlphaToCoverage/App.{h,cpp}`, `WinMain.cpp`, `40_Surface.hlsl`, `40_MSAA_AlphaToCoverage.vcxproj`, `.vcxproj.filters` | 캐릭터·진단 장면·UI와 독립 실행 |
| 5 | `Dx11/TutorialApp.sln`, `Dx11/Directory.Build.targets` | 솔루션과 아이콘 등록 |
| 5 | `tools/readme_media_manifest.json`, `tools/tests/test_readme_media_manifest.ps1` | 40번 미디어 계약 |
| 5 | `Dx11/40_MSAA_AlphaToCoverage/README.md`, `Dx11/39_Transparency_OIT/README.md`, `README.md` | 예제 설명·이전/다음·목록 |
| 5 | 아래 명시한 `docs/media/readme/40-*`, `info/40-*`, `capture-report.md` | 앱 실행 캡처와 촬영 기록 |
| 5 | `docs/project40-pr-draft.md` | 사용자 첨삭용 PR 초안 |

`tools/readme_media_common.ps1`와 `tools/capture_readme_media.ps1`는 읽기만 한다. 40번에 필요한 `1/2/3/C/O/S`는 기존 키 지원으로 처리할 수 있어 변경할 필요가 없다. 실제 결함을 발견하면 원인을 확인하고 영향·테스트를 계획에 명시한 뒤 수정한다.

## 실행·검증 공통 규칙

명령은 저장소 루트에서 실행한다. 테스트 wrapper는 기존 `tools/tests/test_scene_transparency.ps1`, `test_oit_pipeline.ps1`의 환경 정규화·임시 디렉터리 방식을 재사용하되 새 프로젝트 이름과 경로로 작성한다. `PATH`/`Path` 중복 키를 단일 `PATH`로 정규화한다. VS 2022 BuildTools를 우선하고 fallback `vswhere`도 `-version '[17.0,18.0)'`로 제한한다. VS 18의 PCH와 섞지 않는다.

MSBuild 인자는 `/m:1 /nr:false /p:BuildInParallel=false`를 사용한다. 테스트는 `/W4 /WX`, 새 HLSL은 strictness와 warnings-as-errors를 적용한다. Common의 기존 인코딩 때문에 앱에 일괄 `/utf-8`을 강제하지 않는다. 외부 라이브러리의 기존 경고와 새 경고를 구분해 기록한다.

테스트 출력은 현재 작업 전용 기록에 RED/GREEN 명령, 종료 코드, 실패 이유, 검사 수를 남긴다. WARP 4x 미지원은 전용 종료 코드 2와 skip 사유로 보고하고 wrapper가 전체 통과 문구를 출력하지 않게 한다. 지원 장치에서 4x 픽셀 검증을 완료하기 전에는 Task 2를 완료로 표시하지 않는다.

---

## Task 1: 재질 비교 규칙과 독립 테스트

**Files:** 파일 구성의 Task 1 파일 5개를 생성한다. 기존 `SceneTransparency` 파일은 수정하지 않는다.

**Interfaces — Produces:** `CoverageScene.h`에서 다음 타입과 함수를 정의한다. 모든 선언은 `Coverage40` 안에 둔다. `std::size_t`, `std::string_view`, `std::vector`, 고정 폭 정수에 필요한 표준 헤더를 포함한다.

```cpp
enum class Mode { AlphaTest1x, AlphaTest4x, AlphaToCoverage4x };
enum class AlphaMode { Opaque, Mask, Blend };
enum class SurfaceRule { Opaque, Mask, Blend, TestCoverage, AlphaToCoverage };
inline constexpr std::string_view LaceMaterialName =
    "N00_002_01_Tops_01_CLOTH_02 (Instance)";
struct MaterialInput {
    AlphaMode mode{AlphaMode::Opaque};
    float cutoff{0.5f};
    std::size_t index{};
    std::string_view name;
    bool diagnosticPattern{};
};
struct Settings {
    Mode mode{Mode::AlphaTest1x};
    float alphaMultiplier{1.0f};
    float laceCutoff{0.5f};
    float diagnosticCutoff{0.5f};
};
struct EffectiveMaterial {
    SurfaceRule rule{SurfaceRule::Opaque};
    float cutoff{0.5f};
    float opacity{1.0f};
    bool comparisonTarget{};
};
struct TransparentDraw { std::size_t id; float viewDepth; };
bool IsLaceMaterial(const MaterialInput& input);
EffectiveMaterial ResolveMaterial(const MaterialInput& input, const Settings& settings);
void OrderTransparentDraws(std::vector<TransparentDraw>& draws);
std::uint32_t SampleCount(Mode mode);
bool Supports4x(bool colorQueryOk, std::uint32_t colorLevels,
                bool depthQueryOk, std::uint32_t depthLevels, bool requiredFormats);
std::uint64_t TargetBytes(std::uint32_t width, std::uint32_t height,
                          std::uint32_t samples);
```

- [ ] **1.1 실패 테스트와 실행 진입점 작성.** 표준 `assert`가 Release에서 사라지는 문제를 피하도록 실패 시 stderr와 비정상 종료 코드를 내는 기존 native fixture 패턴을 사용한다. 아래는 테스트 본문에 넣을 대표 판정이다.

```cpp
Settings settings;
settings.mode = Mode::AlphaToCoverage4x;
settings.alphaMultiplier = 0.25f;
MaterialInput lace{AlphaMode::Mask, 0.5f, 4, LaceMaterialName, false};
if (!IsLaceMaterial(lace)) return 1;
const auto result = ResolveMaterial(lace, settings);
if (result.rule != SurfaceRule::AlphaToCoverage || result.opacity != 0.25f) return 1;
const auto eye = ResolveMaterial({AlphaMode::Blend, 0.5f, 5, "eye", false}, settings);
if (eye.rule != SurfaceRule::Blend || eye.opacity != 1.0f) return 1;
if (TargetBytes(1600, 900, 1) != 17280000ULL) return 1;
if (TargetBytes(1600, 900, 4) != 80640000ULL) return 1;
```

  테스트 프로젝트 GUID는 `{54391B06-2E47-4E79-AD12-DDECA2596440}`. `CoverageSceneTests.cpp`와 실제 `CoverageScene.cpp`만 컴파일하고 Common/D3D는 링크하지 않는다. wrapper의 temp prefix는 `D3D11-CoverageSceneTests-`, 실행 파일은 `CoverageSceneTests.exe`다.

- [ ] **1.2 RED 확인.** `pwsh -NoProfile -File tools/tests/test_coverage_scene.ps1` 실행. 아직 없는 `CoverageScene.h/cpp` 때문에 빌드가 실패해야 한다. 도구 검색 실패를 기능의 RED로 대신 기록하지 않는다.
- [ ] **1.3 재질 분류 구현.** 레이스는 index/name/MASK 모두 일치해야 한다. 진단 패턴 또는 검증된 레이스만 비교 대상이다. 다른 재질은 원본 규칙·cutoff·opacity 1을 유지한다.

```cpp
const bool target = input.diagnosticPattern || IsLaceMaterial(input);
if (target) {
    return {settings.mode == Mode::AlphaToCoverage4x
                ? SurfaceRule::AlphaToCoverage : SurfaceRule::TestCoverage,
            input.diagnosticPattern ? settings.diagnosticCutoff : settings.laceCutoff,
            std::clamp(settings.alphaMultiplier, 0.0f, 1.0f), true};
}
```

- [ ] **1.4 경계 사례를 테스트로 추가한 뒤 구현.** index만 틀림, 이름 suffix만 틀림, 원본 BLEND, 일반 MASK cutoff 0.3, 진단 cutoff 독립 유지, 세 모드별 규칙을 검사한다. 정렬은 `stable_sort`와 `left.viewDepth > right.viewDepth`를 사용해 동률 제출 순서를 유지한다. `(id,depth)=(0,2),(1,4),(2,4)`의 결과는 `1,2,0`이다.
- [ ] **1.5 지원/메모리 판정 구현.** `Supports4x`는 두 query 성공·두 level 양수·필수 포맷 지원의 AND. 각 조건을 하나씩 false/0으로 바꾼 테스트를 추가한다. `SampleCount`는 첫 모드 1, 나머지 4다. `TargetBytes`는 1 또는 4만 허용하며 0 크기·다른 샘플 수·D3D11 2D 한계 16384 초과는 0을 반환한다.

```cpp
if (!width || !height || width > 16384 || height > 16384 ||
    (samples != 1 && samples != 4)) return 0;
return std::uint64_t(width) * height * (samples == 1 ? 12ULL : 56ULL);
```

- [ ] **1.6 GREEN·리뷰·커밋.** 1.2 명령을 다시 실행하고 모든 분류·정렬·지원·메모리 검사를 확인한다. 커밋 메시지: `feat: add project 40 coverage material rules`. 이 작업의 파일만 지정한다.

## Task 2: MSAA 타깃·알파 처리·Resolve의 실제 픽셀 검증

**Files:** 파일 구성의 Task 2 파일을 생성한다. Task 1 헤더/구현은 링크해 사용한다.

**Interfaces — Consumes:** Task 1의 `Mode`, `SurfaceRule`, `SampleCount`, `Supports4x`, `TargetBytes`.

**Interfaces — Produces:** `MsaaPipeline.h`의 공개 API. `Configure`는 프레임 사이에만 호출하고 호출자가 이전 OM/SRV를 먼저 해제한다. `BeginFrame`은 clear/viewport/타깃을 설정하며 `BeginSurface`는 OM 상태만 설정한다.

```cpp
struct Capabilities { bool supports4x{}; std::wstring reason; };
class MsaaPipeline {
public:
    bool Initialize(ID3D11Device* device, const std::wstring& shaderDirectory);
    bool Configure(ID3D11Device* device, UINT width, UINT height, Mode mode);
    void BeginFrame(ID3D11DeviceContext* context, const float clearColor[4]);
    void BeginSurface(ID3D11DeviceContext* context, SurfaceRule rule);
    void EndScene(ID3D11DeviceContext* context);
    void Resolve(ID3D11DeviceContext* context);
    void Present(ID3D11DeviceContext* context, ID3D11RenderTargetView* output,
                 float exposure);
    const Capabilities& Support() const;
    Mode CurrentMode() const;
    UINT Width() const;
    UINT Height() const;
    UINT Samples() const;
    std::uint64_t MemoryBytes() const;
    ID3D11Texture2D* ColorTexture() const;
    ID3D11Texture2D* DepthTexture() const;
    ID3D11Texture2D* LinearTexture() const;
};
```

`LinearTexture`는 1x에서 ColorTexture와 동일, 4x에서는 별도 Resolve 텍스처다. getter는 소유권을 넘기지 않는다. `EndScene`은 Task 3 이전에는 측정 없는 패스 종료 지점이다.

- [ ] **2.1 WARP fixture 작성.** `OitPipelineTests.cpp`의 장치 생성·shader compile·staging readback 방식을 읽고 OIT 식·타깃은 가져오지 않는다. `MsaaPipelineTests.vcxproj` GUID는 `{EB8A76A5-F4A0-4B65-A28A-07537796B640}`, 링크는 d3d11/dxgi/d3dcompiler와 실제 MsaaPipeline.cpp/CoverageScene.cpp. wrapper temp prefix는 `D3D11-MsaaPipelineTests-`, 실행 파일은 `MsaaPipelineTests.exe`다.

  테스트 전용 `Fixture`는 다음 인터페이스를 구현한다. `Draw`는 fullscreen triangle, 지정된 clip-space z, 상수 색과 alpha rule을 production 공유 HLSL로 렌더링한다. 비교용 삼각형에는 조명·텍스처 필터링을 넣지 않는다. `ReadLinear`는 Resolve 완료된 단일 샘플 텍스처의 (8,8)을 FP16→float로 읽는다. 모든 HRESULT 실패는 예외로 테스트를 실패시킨다.

  `MsaaTestSurface.hlsl`은 `40_AlphaCoverage.fxh`를 상대 경로로 include한다. fixture의 장면 래스터라이저는 CullMode NONE, DepthClipEnable TRUE, MultisampleEnable TRUE이며 `SV_Coverage`나 sample-frequency semantic을 쓰지 않는다. sample mask 평균 검사를 위해 `Fixture::Draw`에 마지막 매개변수 `UINT sampleMask = 0xffffffff`를 추가한다. BeginSurface 이후 현재 blend state/factor를 유지한 채 이 mask를 적용해 draw하고, draw 뒤 전체 mask로 복구한다.

```cpp
struct Color { float r, g, b, a; };
class Fixture {
public:
    void Initialize(const std::filesystem::path& repo);
    MsaaPipeline& Pipeline();
    ID3D11Device* Device();
    ID3D11DeviceContext* Context();
    void Configure(Mode mode); // unbind then Configure(device, 16, 16, mode)
    void Begin(Color background); // pipeline.BeginFrame
    void Draw(Color color, float z, SurfaceRule rule, float cutoff = 0.5f,
              UINT sampleMask = 0xffffffff);
    void Finish(); // pipeline.EndScene then pipeline.Resolve
    Color ReadLinear();
};
```

- [ ] **2.2 첫 실패 픽셀 테스트 실행.** 검사는 `Initialize → Configure(AlphaTest1x) → Begin → Finish → ReadLinear`로 배경 (0.1,0.2,0.3) 보존부터 시작한다. `pwsh -NoProfile -File tools/tests/test_msaa_pipeline.ps1`이 미구현 파이프라인 때문에 실패하는지 확인한다.
- [ ] **2.3 타깃과 Configure 구현.** Initialize에서 format support와 두 포맷의 4x quality를 조회한다. HDR의 `TEXTURE2D | RENDER_TARGET | SHADER_SAMPLE` 기본 지원, 4x의 `MULTISAMPLE_RENDERTARGET | MULTISAMPLE_RESOLVE`, 깊이의 `DEPTH_STENCIL` 지원을 구분한다. 기본 지원 실패는 초기화 실패, 4x만 실패하면 1x를 유지한다. HRESULT와 format/count를 오류에 포함한다.

```cpp
D3D11_TEXTURE2D_DESC desc{};
desc.Width = width; desc.Height = height;
desc.MipLevels = 1; desc.ArraySize = 1;
desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
desc.SampleDesc = {SampleCount(mode), 0};
desc.Usage = D3D11_USAGE_DEFAULT;
desc.BindFlags = D3D11_BIND_RENDER_TARGET;
if (desc.SampleDesc.Count == 1) desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
```

  깊이는 `D32_FLOAT`/`BIND_DEPTH_STENCIL`로 같은 sample desc를 사용한다. 4x에서는 Count 1인 HDR texture+SRV를 추가한다. 모두 지역 ComPtr에 생성한 뒤 성공 시 멤버를 교체한다. 같은 크기의 두 4x 모드 전환은 리소스 주소를 유지한다. 0 크기·16384 초과·미지원 4x는 기존 구성과 리소스를 변경하지 않고 false다.
- [ ] **2.4 공유 알파 함수와 OM 상태 구현.** `SurfaceRule`의 순서 0–4를 HLSL 주석/상수에 명시한다. `40_AlphaCoverage.fxh`의 `float ApplyCoverageAlpha(float alpha, uint rule, float cutoff)`를 앱과 fixture에서 호출한다.

```hlsl
float ApplyCoverageAlpha(float alpha, uint rule, float cutoff)
{
    alpha = saturate(alpha);
    if (rule == 0) return 1; // Opaque
    if (rule == 1) { clip(alpha - cutoff); return 1; } // authored Mask
    if (alpha <= 0) discard;
    if (rule == 3) { clip(alpha - cutoff); return 1; } // comparison TestCoverage
    return alpha; // Blend or AlphaToCoverage
}
```

  Opaque/Mask/TestCoverage는 일반 블렌딩·A2C OFF, 깊이 LESS·쓰기 ALL. AlphaToCoverage는 일반 블렌딩 OFF, A2C ON, 깊이 쓰기 ALL. Blend는 A2C OFF, RGB SRC_ALPHA/INV_SRC_ALPHA, alpha ONE/INV_SRC_ALPHA, 깊이 LESS·쓰기 ZERO. sample mask는 `0xffffffff`. 비교 표면은 straight RGB를 출력하며 A2C에서 RGB에 알파를 곱하지 않는다. fullscreen Present는 깊이·블렌딩·A2C를 끈다.
- [ ] **2.5 Resolve·Present 구현.** Resolve 전에 OM 바인딩을 해제한다. 1x는 복사하지 않는다. 4x는 아래 API로 평균을 만들고 같은 결과를 UI 전 화면 변환에 사용한다.

```cpp
context->ResolveSubresource(resolvedTexture_.Get(), 0, colorTexture_.Get(), 0,
                            DXGI_FORMAT_R16G16B16A16_FLOAT);
```

  `40_Present.hlsl`의 `VSFullscreen`/`PSPresent`를 엄격 컴파일한다. PS는 `Load(int3(SV_POSITION.xy,0))`로 같은 크기의 linear texture를 읽고 `x=max(rgb*exposure,0); y=x/(1+x)` 후 채널별 `y<=0.0031308 ? 12.92*y : 1.055*pow(y,1/2.4)-0.055`를 출력한다. alpha는 1. 입력과 다른 UNORM 출력 RTV를 먼저 바인딩하고 그 뒤 SRV를 연결한다. 종료 후 입력 SRV를 해제한다.
- [ ] **2.6 깊이·커버리지 실패 테스트를 추가하고 통과시키기.** 모든 RGB 비교 허용 오차는 linear FP16 0.003, 최종 UNORM 2/255로 명시한다. 다음 검사를 이름별로 보고한다.

  - Alpha Test 1x/4x: alpha 0.49는 cutoff 0.5에서 배경, 0.51은 전경. 알파 0/cutoff 0 뒤에 그린 불투명 면이 보이며 깊이가 오염되지 않는다.
  - A2C 4x: alpha 0은 배경과 깊이 보존, alpha 1은 완전한 전경. 불투명 z=0.2 뒤의 A2C z=0.6은 가려진다.
  - 부분 커버리지: alpha=1/32…31/32를 순회해 전경 빨강 z=0.3, 배경 검정에서 0<r<1인 케이스를 찾는다. 같은 alpha의 빨강 뒤에 파랑 z=0.6을 그린 결과가 `(r,0,1-r)`인지 검사한다. 하드웨어의 특정 비트 패턴을 가정하지 않는다.
  - 상태 복원: A2C alpha 0 draw 뒤에 Blend 초록 alpha 0.25/z=0.2를 검정 위에 그리면 RGB `(0,0.25,0)`다. 깊이 쓰기 OFF도 뒤쪽 불투명 draw로 별도 확인한다.
  - 일반 MASK의 alpha 0/cutoff 0은 원본 clip 의미대로 남고, 비교 TestCoverage만 버리는 차이를 검사한다.

```cpp
fixture.Configure(Mode::AlphaToCoverage4x);
fixture.Begin({0, 0, 0, 1});
fixture.Draw({1, 0, 0, 0}, 0.3f, SurfaceRule::AlphaToCoverage);
fixture.Draw({0, 1, 0, 0.25f}, 0.2f, SurfaceRule::Blend);
fixture.Finish();
const Color value = fixture.ReadLinear();
if (std::abs(value.g - 0.25f) > 0.003f || std::abs(value.r) > 0.003f) return 1;
```

- [ ] **2.7 Resolve·구성 변경 검사.** fixture Draw의 sampleMask에 `1u<<sampleIndex`를 전달해 각 샘플에 빨강·초록·파랑·흰색을 alpha=1로 그린다. alpha=1이면 A2C가 전체 커버리지를 제공하므로 최종 mask는 지정한 샘플만 남는다. production Resolve 결과 RGB가 `(0.5,0.5,0.5)`인지 확인한다. 빈 배경의 Present를 별도 단일 샘플 UNORM 타깃에 출력해 위 변환식과 비교한다.

  두 4x 모드 사이 ColorTexture 주소 유지, 1x의 ColorTexture==LinearTexture, 4x의 두 주소 다름, 16→32 크기 변경 후 정상 픽셀을 검사한다. `fixture.Pipeline().Configure(fixture.Device(), 0, 16, Mode::AlphaTest1x)`와 width만 16385인 호출이 거부된 뒤 이전 구성이 유지되는지도 검사한다. 20회 모드 전환을 수행하고 매 프레임 Begin/Finish로 이전 프레임 결과가 섞이지 않게 한다.
- [ ] **2.8 GREEN·디버그 메시지·커밋.** 2.2 명령과 Task 1 suite 재실행. debug layer가 있으면 `ID3D11InfoQueue`에서 ERROR/CORRUPTION을 실패로 처리하고 WARNING도 수집·원인 확인한다. debug device 생성이 `DXGI_ERROR_SDK_COMPONENT_MISSING`인 경우만 비-debug로 재시도하고 미수집 상태를 보고한다. 커밋: `feat: implement MSAA alpha-to-coverage pipeline`.

## Task 3: 구성별 GPU 시간 측정

**Files:** `MsaaPipeline.h/.cpp`, `tools/tests/native/MsaaPipelineTests.cpp` 수정.

**Interfaces — Consumes:** Task 2의 BeginFrame/EndScene/Resolve/Configure.

**Interfaces — Produces:** 다음 타입과 `const GpuTimings& MsaaPipeline::Timings() const` getter를 추가한다.

```cpp
struct GpuTimings {
    bool available{}, valid{}, resolveApplicable{};
    Mode mode{Mode::AlphaTest1x};
    UINT width{}, height{};
    double sceneMs{}, resolveMs{};
};
```

- [ ] **3.1 실패 검사 추가.** Configure 직후 valid=false, 1x resolveApplicable=false, 4x true, 모드·크기 정보 일치 검사를 추가한다. 새 API 미구현 상태에서 `pwsh -NoProfile -File tools/tests/test_msaa_pipeline.ps1`의 실패를 확인한다.
- [ ] **3.2 쿼리 링 구현.** 4슬롯 각각에 disjoint와 scene begin/end, resolve begin/end timestamp, inFlight, 구성 generation을 둔다. BeginFrame의 clear 직전에 scene 시작, EndScene에서 scene 종료, Resolve API 앞뒤에서 resolve 시간을 찍는다. 1x는 Resolve timestamp를 실행하지 않고 disjoint만 끝낸다. Present는 측정 밖이다.

```cpp
const HRESULT status = context->GetData(query.Get(), &data, sizeof(data),
                                        D3D11_ASYNC_GETDATA_DONOTFLUSH);
if (status == S_FALSE) return; // 다음 프레임에서 확인, 기다리지 않음
```

  여기서 `query/data`는 해당 timestamp 또는 disjoint의 실제 타입에 맞춘 지역 변수다. 오류 HRESULT는 선택 측정을 비활성화한다. 미완료 슬롯은 재사용하지 않고 빈 슬롯이 없으면 해당 프레임 측정을 건너뛴다. disjoint=true 또는 frequency=0 결과는 버린다. Configure 성공 시 generation을 증가시키고 valid를 지운다. 기존 inFlight 쿼리는 완료까지 배출하되 다른 generation 결과는 게시하지 않는다.
- [ ] **3.3 준비·완료·전환 검증.** fixture에서 최대 120프레임 동안 렌더하고 테스트 측에서만 Flush/양보해 비동기 결과를 관찰한다. valid 결과는 sceneMs/resolveMs가 finite·음수 아님, mode/width/height가 현재 구성과 일치해야 한다. 1x resolveMs는 0이며 UI에서는 N/A로 사용한다. 장치가 query를 제공하지 않으면 명확히 skip하고 타깃·픽셀 검사는 계속한다. 1x→4x→다른 4x→리사이즈 직후 stale valid가 없는지 검사한다.

  query 생성은 성공했지만 관찰 기간 내 valid가 한 번도 되지 않으면 측정 완료 검사는 실패로 보고한다. 단순히 valid 분기를 건너뛰고 통과시키지 않는다. 하드웨어/스케줄링 지연인지 조사할 때도 앱 자체의 비동기 경로에 대기 루프를 추가하지 않는다.
- [ ] **3.4 GREEN·리뷰·커밋.** GPU suite 전체 통과와 비동기 경로 코드 검토: polling 루프에 대기 없음, 미완료 슬롯 덮어쓰기 없음, 이전 generation 게시 없음. 커밋: `feat: measure project 40 scene and resolve GPU time`.

## Task 4: 캐릭터·진단 장면과 실행 앱

**Files:** 파일 구성의 Task 4 파일을 생성하고, `tools/tests/native/MsaaPipelineTests.cpp`에 앱 셰이더 컴파일 검사를 추가한다. 실제 GLB와 Common 파일은 수정하지 않는다.

**Interfaces — Consumes:** Task 1 `Settings/ResolveMaterial/OrderTransparentDraws`, Task 2 전체 API, Task 3 `Timings`.

**Interfaces — Produces:** `App final : public GameApp`, `OnInitialize/OnUninitialize/OnUpdate/OnRender/OnInputProcess/WndProc` override는 기존 39번 App.h와 동일 시그니처를 사용한다. private helper는 `CreateDevice`, `CreateSurfaceResources`, `LoadCharacter`, `BuildPosedCentroids`, `Resize`, `BuildDraws`, `DrawSurface`, `RenderHud`, `HandleKey`, `Fail`로 나눈다.

- [ ] **4.1 앱 셰이더 실패 검사 추가.** native GPU fixture의 shader compilation 목록에 새 `40_Surface.hlsl`의 `VSMain`/`PSMain`을 추가하고 파일 미존재 실패를 확인한다. fixture는 테스트 표면뿐 아니라 이 진입점도 매번 엄격 컴파일한다.
- [ ] **4.2 표면 셰이더 구현.** 39번 surface shader의 96바이트 skinned vertex layout과 row-major bone palette를 따른다. 아래 cbuffer는 App의 alignas(16) 구조체와 필드 순서를 맞춘다. 앱 구조체에 sizeof가 16 배수인지 static_assert를 둔다.

```hlsl
#include "40_AlphaCoverage.fxh"
cbuffer Surface : register(b0) {
    float4x4 world, view, projection;
    float4 baseColor;
    float4 material; // SurfaceRule, cutoff, opacity, skinning
    float4 surface;  // texture present, manual sRGB decode, 0, 0
};
cbuffer Bones : register(b1) { row_major float4x4 bonePalette[1023]; }
```

  VS는 `mul(position,skin)` 후 world/view/projection을 적용한다. PS는 기존 texture SRV가 sRGB인지 확인해 필요한 경우에만 RGB를 수동 decode, texture×factor×vertexColor의 alpha에 opacity를 곱한 뒤 ApplyCoverageAlpha를 호출한다. 고정 조명은 39번과 같은 ambient 0.48 + diffuse 0.72, 방향 (-0.35,0.7,-0.6)이다. 별도 OIT 출력·HDR 누적 코드는 넣지 않는다.
- [ ] **4.3 모델과 고정 포즈 연결.** `..\Resource\fbx\Public\MyAlice\Player\SampleModel.glb`를 FbxModel로 읽고 39번의 CharacterAnimator 사용 경로로 `VRM_1` 0.5초를 평가한다. 스키닝에 업로드한 동일 palette로 중심을 계산한다. factor 중복 곱셈 여부는 39번 LoadCharacter와 Common getter를 읽어 확인한다. 재질 인덱스/이름/MASK가 일치하면 laceCutoff를 원본 cutoff로 초기화한다. 실패하면 HUD에 비교 비활성 이유를 표시하고 원본 재질과 진단 장면을 유지한다.
- [ ] **4.4 진단 geometry/texture와 draw 목록 생성.** 불투명 대각선 사각형, 알파 패턴 사각형, 앞쪽 불투명 가림판을 별도 draw로 둔다. 256×256 RGBA8 패턴은 RGB 흰색, 상반부 x방향 alpha ramp, 하반부 가는 사선 줄무늬로 생성한다. mip은 1개, linear clamp sampler로 고정해 mip 정책 변경을 비교에 섞지 않는다.

```cpp
const float u = float(x) / 255.0f;
const float stripe = ((x + y) % 16 < 8) ? 1.0f : 0.0f;
const float alpha = y < 128 ? u : stripe;
const auto byteAlpha = static_cast<std::uint8_t>(std::lround(alpha * 255.0f));
```

  비교 대상과 일반 MASK/불투명 draw를 먼저 처리하고 `SurfaceRule::Blend`만 하나의 전역 안정 정렬 목록에 넣는다. 재질 `doubleSided`에 따라 culling을 설정하고 두 scene rasterizer의 MultisampleEnable은 TRUE로 둔다. 1x/4x가 같은 shader·조명을 사용한다.
- [ ] **4.5 앱 프레임 연결.** `settings.mode`는 Configure 성공한 모드에서만 바꾼다. 초기화는 AlphaTest1x, 레이스 확대, orbit OFF, alpha 1, exposure 1. 장면 전환은 Character/Diagnostic 두 개다. C는 Character로 이동해 확대 상태를 토글하며 카메라 선택이 모드 전환 때문에 바뀌지 않게 한다.

```cpp
pipeline_.BeginFrame(context_.Get(), background_);
for (const auto& draw : opaqueDraws_) {
    pipeline_.BeginSurface(context_.Get(), draw.material.rule);
    DrawSurface(draw);
}
for (const auto& sorted : transparentDraws_) {
    const auto& draw = draws_[sorted.id];
    pipeline_.BeginSurface(context_.Get(), SurfaceRule::Blend);
    DrawSurface(draw);
}
pipeline_.EndScene(context_.Get());
pipeline_.Resolve(context_.Get());
pipeline_.Present(context_.Get(), backbuffer_.Get(), exposure_);
```

  `opaqueDraws_`는 유효 material을 가진 Draw 목록, `draws_`는 정렬 대상 원본 Draw 목록이다. ImGui 렌더와 swapchain Present는 위 구간 뒤에 실행한다. UI를 프레임 시작에 갱신해 선택이 해당 프레임에 반영되게 한다.
- [ ] **4.6 입력·실패 처리와 HUD.** 1600×900, DPI-aware 창과 오른쪽 HUD 구도를 사용한다. 1/2/3/C/O/S와 동일 ImGui 컨트롤, cutoff(A2C에서 disabled), 알파 배율, 배경, 노출을 제공한다. WM_KEYDOWN은 repeat bit와 WantCaptureKeyboard를 확인한다. 4x 미지원은 combo/키 모두 차단한다. sample count는 Pipeline::Samples에서 표시하고 MemoryBytes는 MiB로 환산해 논리적 타깃 추정이라고 적는다. timings는 valid/available과 N/A를 구분한다.

  최소화는 건너뛰고 WM_SIZE에서 OM/SRV·백버퍼 참조를 해제한 뒤 ResizeBuffers/타깃 재구성한다. 타깃 생성 실패로 크기가 다르면 이후 유효 resize 성공 전까지 draw하지 않는다. 필수 초기화·Present/device removal 실패는 작업명/HRESULT와 함께 알리고 종료한다. 쿼리 미지원은 종료 이유가 아니다. 기존 DX11_README_CAPTURE와 imgui.ini 저장/복원 관례를 유지한다.
- [ ] **4.7 프로젝트 파일과 빌드.** 앱 GUID `{D925C03C-9331-47A7-98D5-17975AF40F40}`, Common reference `{05774CF5-5EB5-455B-8ADF-707FB11F2F9F}`. 39번 프로젝트의 Debug/Release x64, v143/C++20, resource copy를 따르고 40번의 모든 cpp와 세 shader 파일을 등록·출력 복사한다. 아직 솔루션 등록은 Task 5에서 한다.

  환경을 정규화한 ProcessStartInfo에서 다음 ArgumentList로 VS 2022 MSBuild를 실행한다. stdout/stderr를 모두 비동기로 읽고 종료 코드와 로그를 보존한다.

```text
Dx11/40_MSAA_AlphaToCoverage/40_MSAA_AlphaToCoverage.vcxproj
/t:Build /p:Configuration=Debug /p:Platform=x64
/p:SolutionDir=C:\Github\D3D11-AliceTutorial\Dx11\
/m:1 /p:BuildInParallel=false /nr:false /v:minimal
```

  링크된 worktree라면 SolutionDir만 해당 저장소의 Dx11 절대 경로로 바꾼다. Release도 Configuration만 바꾸어 실행한다.
- [ ] **4.8 런타임·회귀·커밋.** Task 1/2 suite와 기존 `test_oit_pipeline.ps1`, `test_scene_transparency.ps1` 실행. 앱을 열어 세 모드·두 장면·확대·orbit·리사이즈·최소화/복원·눈 재질·카메라 유지·측정 표시를 확인한다. 실패는 해당 테스트나 재현 절차를 먼저 추가하고 수정한다. 커밋: `feat: add project 40 character coverage comparison app`.

## Task 5: 프로젝트 연결·실행 자료·PR 초안

**Files:** 파일 구성의 Task 5 경로. 생성 미디어의 정확한 이름:

- `docs/media/readme/40-MSAA-AlphaToCoverage.png`
- `docs/media/readme/40-MSAA-AlphaToCoverage.gif`
- `docs/media/readme/info/40-MSAA-AlphaToCoverage-info.png`
- `docs/media/readme/40-MSAA-AlphaToCoverage-alpha-test-1x-lace.png`
- `docs/media/readme/40-MSAA-AlphaToCoverage-alpha-test-4x-lace.png`
- `docs/media/readme/40-MSAA-AlphaToCoverage-a2c-4x-lace.png`

**Interfaces — Consumes:** Task 4 executable, `1/2/3/C/O/S` 입력, 기존 capture manifest schema/도구.

- [ ] **5.1 manifest 실패 검사 작성.** `test_readme_media_manifest.ps1`에서 capture-mode 목록에 40을 추가하고 다음 실제 객체 판정을 넣는다. 실행 후 아직 없는 40번 때문에 실패를 확인한다.

```powershell
$project40 = @($manifest.projects | Where-Object number -eq '40')
Assert-True ($project40.Count -eq 1) 'project 40 must appear exactly once'
Assert-True ($project40[0].directory -ceq '40_MSAA_AlphaToCoverage') 'project 40 directory'
Assert-True ([bool]$project40[0].readmeCaptureMode) 'project 40 capture mode'
Assert-True ($project40[0].gifPhase -ceq 'runtime') 'project 40 runtime GIF'
```

- [ ] **5.2 솔루션·branding·manifest 등록.** Task 4 GUID를 솔루션에 등록한다. 기존 39번처럼 x86 solution ActiveCfg는 유효 x64에 연결하되 x86 Build.0은 넣지 않는다. Directory.Build.targets의 AliceTutorialBrandingProjects에 40번 이름을 추가한다. manifest expectedProjectCount는 40으로 바꾸고 아래 entry를 추가한다. 다른 프로젝트 capture 설정은 보존한다.

```json
{"number":"40","name":"MSAA Alpha-to-Coverage","directory":"40_MSAA_AlphaToCoverage","exe":"40_MSAA_AlphaToCoverage.exe","image":"40-MSAA-AlphaToCoverage.png","gif":"40-MSAA-AlphaToCoverage.gif","infoImage":"info/40-MSAA-AlphaToCoverage-info.png","delayMs":3000,"readmeCaptureMode":true,"title":"MSAA & Alpha-to-Coverage","summary":"레이스의 알파 경계를 Alpha Test와 MSAA, Alpha-to-Coverage로 비교합니다.","tags":["MSAA","Alpha-to-Coverage","Resolve","Character"],"gifPhase":"runtime","gifActions":[{"atMs":0,"type":"keyDown","key":"O"},{"atMs":3000,"type":"keyUp","key":"O"}]}
```

  대표 PNG·GIF는 기본 1x 상태를 정확히 표시한다. 비교용 세 PNG는 따로 모드를 전환해 촬영한다. 미지원 4x를 4x 비교 캡처로 게시하지 않는다.
- [ ] **5.3 README 초안과 생성 연결.** 40번 README에 NAV/INFO/RUNTIME marker를 기존 순서로 넣고 설계의 여섯 항목 구성으로 작성한다. 기본 그림과 비교 이미지 링크, 실제 production code·테스트 경로, 공식 A2C/Resolve 문서 링크를 넣는다. root의 학습 흐름·현재 예제 안내를 40번까지 갱신하고 39번은 다음 링크만 변경한다. 성능 수치나 촬영 결과는 아직 완료했다고 쓰지 않는다.

```powershell
pwsh -NoProfile -File tools/generate_readme_info_images.ps1 -ProjectNumber 40
pwsh -NoProfile -File tools/update_project_readmes.ps1 -All
pwsh -NoProfile -File tools/update_readme_branding.ps1
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
```

  updater의 두 번째 실행이 추가 변경을 만들지 않는지 확인한다. 1–38번 문서가 변경되면 자동 생성 범위를 확인해 불필요한 변경을 배제한다. 사용자 수정은 덮어쓰지 않는다.
- [ ] **5.4 앱 촬영.** 먼저 `pwsh -NoProfile -File tools/capture_readme_media.ps1 -ProjectNumber 40 -ValidateOnly`로 계약을 확인한다. 새 exe·shader를 manifest runtimeDir에 배치한 후 `-OutputDir`에 작업 전용 캡처 폴더를 지정해 실제 촬영한다. 기존 루트 capture-report를 단일 프로젝트 결과로 덮어쓰지 않는다.

  세 비교 PNG는 같은 창 크기·카메라·배경·알파·cutoff·조명·고정 포즈에서 1/2/3을 각각 눌러 찍는다. 각 키는 down→125ms 이상→up으로 전달하며 모드 변경 이후 안정된 프레임을 기다린다. OIT 실험용 L/R 입력은 사용하지 않는다. PNG는 1600×900, GIF는 800×450/4초, info는 1600×640 계약을 유지한다.

  생성 결과와 GIF의 앞/중간/끝 프레임을 이미지 도구로 열어 HUD 모드·샘플 수, 레이스 가림·잘림, DPI 선명도와 실제 카메라 움직임을 확인한다. 승인할 캡처만 지정된 게시 경로로 복사하고 현재 촬영 결과의 40번 행을 기존 capture-report에 추가한다. 이전 행과 날짜는 보존한다. 캡처 락·imgui.ini·마우스 상태를 복구하고 작업에서 시작한 앱만 닫는다.
- [ ] **5.5 관찰 결과·PR Markdown 작성.** README의 같은 구도 결과를 관찰한 내용으로 확정한다. A2C가 시간축 깜빡임을 완전히 해결한다거나 모든 각도에서 낫다고 쓰지 않는다. GPU 구간과 메모리 산식의 제외 항목을 적는다. `docs/project40-pr-draft.md`에 제목, 추가 이유, 구현 내용, 레이스 처리, 문제 상황과 확인 방법, 검증 결과, 한계와 이후 계획을 작성한다. 실행하지 못한 검사는 미실행이라고 구분한다. Orca 첨삭은 해당 문서의 리뷰 메모까지 읽어 반영한다.
- [ ] **5.6 전체 회귀와 최종 커밋.** 아래 명령을 실행하고 새 프로젝트 Debug/Release를 다시 빌드한다. 각 스크립트 종료 코드가 0인지 개별 확인한다. 마지막 명령만 성공한 것을 전체 성공으로 해석하지 않는다.

```powershell
pwsh -NoProfile -File tools/tests/test_coverage_scene.ps1
pwsh -NoProfile -File tools/tests/test_msaa_pipeline.ps1
pwsh -NoProfile -File tools/tests/test_oit_pipeline.ps1
pwsh -NoProfile -File tools/tests/test_scene_transparency.ps1
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
pwsh -NoProfile -File tools/tests/test_capture_manifest_actions.ps1
pwsh -NoProfile -File tools/tests/test_project_readme_updater.ps1
pwsh -NoProfile -File tools/tests/test_verify_readme_media.ps1
pwsh -NoProfile -File tools/tests/test_readme_info_images.ps1
pwsh -NoProfile -File tools/tests/test_visual_capture_contracts.ps1
pwsh -NoProfile -File tools/tests/test_update_readme_branding.ps1
pwsh -NoProfile -File tools/tests/test_app_branding.ps1
pwsh -NoProfile -File tools/verify_readme_media.ps1
git -c safe.directory=C:/Github/D3D11-AliceTutorial -c core.safecrlf=false diff --check
```

  새 README/설계/계획/PR의 상대 링크를 실제 파일 경로로 resolve해 누락을 확인한다. 전체 diff를 검토해 Common·기존 renderer·에셋 변경이 없음을 확인한다. 체크리스트와 spec 범위를 대조한 뒤 커밋: `docs: integrate project 40 lesson and captured comparisons`. PR 초안은 사용자에게 파일 링크로 전달하고 GitHub PR은 생성하지 않는다.

## 완료 판정과 인계

- [ ] Task 1–5의 체크리스트와 RED/GREEN·빌드·실행 근거가 기록되었다.
- [ ] 지원 장치의 실제 4x 경로와 동일 구도 비교 이미지가 검증되었다.
- [ ] 원본 BLEND와 비교 레이스의 처리 구분, 실패 상태, 측정 한계가 코드·UI·README에서 일치한다.
- [ ] 변경 파일 범위와 git 상태를 확인하고 남은 미실행 항목을 보고했다.
- [ ] PR Markdown을 전달했으며, 푸시·PR 생성·머지는 사용자 요청을 기다린다.

이 문서는 실행 계획이다. 현재 체크박스는 모두 미완료이며 소스·테스트·미디어가 이미 만들어졌다는 뜻이 아니다.
