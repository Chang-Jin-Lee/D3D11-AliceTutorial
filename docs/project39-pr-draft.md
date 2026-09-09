# feat: 39번 반투명 비교 예제 추가 — Alpha Test, Alpha Blend, Weighted OIT

### 추가한 이유

DX11의 깊이 테스트와 알파 블렌딩을 실제 캐릭터 재질로 비교하기 위해 39번 예제를 추가했습니다.

직접 만든 VRM 캐릭터를 GLB로 내보내 사용하고 있으며, 치마 레이스에 중간 알파값이 있어 비교 대상으로 삼았습니다. 20번의 깊이·알파 문제, 35번의 MRT, 38번의 캐릭터 재질과 반투명 정렬에서 이어지는 내용입니다.

핵심은 **“반투명 면의 깊이 쓰기를 꺼도 왜 그리는 순서에 따라 색이 달라지는가?”**를 확인하는 것입니다.

### 주요 변경

- `39_Transparency_OIT` 프로젝트를 추가하고 솔루션에 등록했습니다.
- 같은 캐릭터·고정 포즈·조명에서 Alpha Test, Sorted Alpha Blend, Weighted Blended OIT를 전환할 수 있습니다.
- 교차하는 반투명 사각형 3장과 불투명 가림판으로 순서 의존성과 깊이 테스트를 비교합니다.
- Accumulation과 Revealage를 서로 다른 MRT 블렌드 상태로 누적하고, 선형 HDR 합성 후 노출·톤매핑·sRGB 변환을 적용합니다.
- 레이스 확대, 제출 순서 역전, 카메라 회전, 누적 버퍼 보기와 CPU 정렬/GPU 패스 시간 표시를 추가했습니다.
- README에 구현 설명과 실제 실행 PNG·GIF, 같은 구도의 레이스 비교 화면을 추가했습니다. 프로젝트 목록·내비게이션과 캡처 도구도 39번에 맞게 연결했습니다.

### 원본 재질과 실험의 구분

원본 GLB의 레이스 재질은 `MASK`입니다. 기본 실행에서는 원래 재질 모드와 cutoff를 따르며, `Lace MASK -> BLEND`를 켰을 때만 지정한 레이스 재질을 반투명으로 처리합니다. 재질 인덱스와 전체 이름을 확인해 다른 재질에 실험 설정이 적용되지 않도록 했습니다.

모델·텍스처 파일과 기존 Common/Animation, 1–38번 렌더링 코드는 변경하지 않았습니다. 38번 README와 공통 프로젝트 등록·문서 도구는 수정했습니다.

### 실행하며 확인할 부분

1. x64 구성에서 `39_Transparency_OIT`를 시작 프로젝트로 선택해 실행합니다.
2. `L`로 레이스 실험을 켜고 `C`로 확대한 뒤, `1/2/3`으로 세 모드를 비교합니다.
3. `S`로 사각형 장면을 선택하고 `R`로 제출 순서를 뒤집어 봅니다. Sorted 모드의 R은 정렬된 목록도 뒤집는 의도적인 실패 실험입니다.
4. `O`로 카메라를 회전하거나, Weighted OIT 모드에서 Accumulation/Revealage 보기를 확인합니다.

고정된 정면 레이스 캡처에서는 Alpha Test의 거친 경계와 블렌딩의 부드러운 중간 알파가 구분됩니다. Sorted와 OIT의 결과는 이 구도에서 서로 비슷하므로, 순서에 따른 차이는 교차 사각형과 역전 실험으로 확인합니다.

### 검증한 내용

- Debug / Release x64 빌드 통과.
- 실제 OIT 코드와 HLSL을 실행하는 WARP 픽셀 테스트 8개 통과.
- 재질 분류·레이스 적용 조건·정렬 동작 검사 17개 통과.
- HDR 색 제한을 제거하면 새 회귀 테스트가 실패하는 것을 확인하고, 셰이더 복원 후 다시 통과했습니다.
- 캡처 단축키, 미디어 manifest, README 생성·미디어 검사 통과.
- 모드 전환, 누적 버퍼 보기, 최소화·복원, 크기 변경과 카메라 회전을 확인했습니다.

주요 자동 검사는 저장소 루트에서 실행할 수 있습니다.

```powershell
pwsh -NoProfile -File tools/tests/test_oit_pipeline.ps1
pwsh -NoProfile -File tools/tests/test_scene_transparency.ps1
pwsh -NoProfile -File tools/tests/test_capture_manifest_actions.ps1
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
pwsh -NoProfile -File tools/verify_readme_media.ps1
```

### 한계와 남은 사항

- Weighted Blended OIT는 정확한 픽셀별 정렬 합성이 아닌 근사법입니다. 특히 불투명에 가까운 층이 겹치면 색이 섞일 수 있습니다.
- FP16 누적을 사용하며, 임의로 많은 층에서 overflow가 발생하지 않는다고 보장하지 않습니다. MSAA/Alpha-to-Coverage와 반투명 외곽선·그림자는 이번 범위에서 제외했습니다.
- 표시되는 GPU 시간은 해당 패스의 시간이며 앱 전체 성능이나 다른 GPU의 성능을 대표하지 않습니다.
- Release 빌드의 기존 Common VMD 중복 정의 경고(`LNK4006`) 6건은 남아 있습니다. 이번 PR에서 수정한 부분은 아닙니다.
- D3D11 디버그 메시지 큐를 수집해 경고가 없는지까지 검증하지는 않았습니다.
