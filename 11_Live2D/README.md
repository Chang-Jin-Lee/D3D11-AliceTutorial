## 11. Live2D (11_Live2D)

<p align="center">
  <img src="https://github.com/user-attachments/assets/3ac3d5cd-45b5-4ab1-be59-a25456c0ee9b" width="60%" />
</p>

- 내용: D3D11 + Live2D Cubism 데모. model3.json 로드, 텍스처 바인딩, 마스크 처리, 모션 재생, ImGui 제어 구현.
- 참고: SDK 경로 설정이 가장 어렵습니다. Framework.lib, src 등의 경로를 잘 보고 설정하세요.
- 주요 구현:
  - Cubism Framework 연동: InitializeConstantSettings, GenerateShader, StartFrame/EndFrame 호출로 D3D11 파이프라인과 동작 연계
  - 모델 로드: `model3.json` 경로 선택 → Core/Framework 생성 → Renderer(D3D11) 준비 → 텍스처 SRV 바인딩(수명 보존)
  - 렌더링: 클리핑 마스크(오프스크린) 생성/사용, MVP 적용, 기본 렌더 상태 설정
  - UI: 파라미터/파트 목록에 ID 문자열(예: ParamAngleX, PartArmL)과 인덱스 표시, 슬라이더로 조정 가능
- 모션 로딩 전략:
  - 설정 그룹(setting): `model3.json`이 정의한 그룹/모션 파일 사용
  - auto 그룹: 모델 폴더에서 `*.motion3.json` 자동 스캔하여 그룹이 비어도 사용 가능
  - extra 그룹: UI의 “Add Motion JSON...”으로 외부 `*.motion3.json` 파일을 개별 등록(모델 폴더 외 경로 지원)
- Motion Player(재생 UI):
  - Group 콤보 선택 → 해당 그룹의 모든 모션을 리스트로 표시(파일명/ID)
  - 슬라이더/리스트 선택으로 모션 선택, 항목별 Play 버튼으로 재생
- UI 구성:
  - Live2D: 모델 로드/상태 + 외부 모션 추가 버튼
  - Live2D Model Info: 모션 재생, 파라미터/파트 조정(이름 표기), 드로어블 카운트 등
  - Live2D Textures: 로드된 텍스처 미리보기
  - System Info: FPS/GPU/CPU/RAM/VRAM
- 의존성/경로:
  - Include: `$(SolutionDir)third_party/CubismSdkForNative-5-r.4.1/Framework/src`, `Core/include`
  - Lib: `$(SolutionDir)third_party/CubismSdkForNative-5-r.4.1/Core/lib/windows/x86_64/143`
  - Link: `Live2DCubismCore_MTd.lib`(Debug) / `Live2DCubismCore_MT.lib`(Release) + 샘플 Framework.lib 또는 직접 소스 컴파일 방식 중 선택
  - DLL: 실행 폴더에 `Live2DCubismCore.dll` 필요
  - Shader: `FrameworkShaders/CubismEffect.fx`가 실행 경로 기준으로 읽히도록 배치
- 사용법:
  1) 앱 실행 후 “Open model3.json”으로 모델 선택
  2) 모션: Group 선택 → 리스트/슬라이더로 모션 선택 → Play
  3) 외부 모션 추가: “Add Motion JSON...” 클릭 → `*.motion3.json` 선택(auto/extra에 반영)
  4) Parameters/Parts에서 슬라이더로 실시간 조정(이름/인덱스 표시)
