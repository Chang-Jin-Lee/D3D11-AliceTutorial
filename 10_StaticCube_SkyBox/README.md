## 10. Static Cube SkyBox (10_StaticCube_SkyBox)
- 내용 : 정적 큐브 메시로 스카이박스를 구성해 배경으로 렌더링
- 주요 구현
  - `g_WorldViewProj = (P · V0 · Fz)^T`
  - `V0`: 뷰 행렬에서 평행이동 제거(회전만 유지)
  - `Fz`: 축 플립 행렬 `diag(1, 1, -1)` (LH 좌표계와 큐브맵 페이스 정합)
  - VS : `posH = mul(float4(posL,1), g_WorldViewProj);` → `posH.xyww`로 원근 영향 제거
  - PS : `g_TexCube.Sample(g_Sam, normalize(posL));` (큐브맵을 방향 벡터로 샘플)
  - 렌더 상태 :
  - Depth: `LESS_EQUAL`, Write OFF
  - Rasterizer: `CullFront`(내부면 렌더)
  - RS/DS 상태 저장/복구로 기존 지오메트리 영향 없음
  - 샘플러/리소스: Linear + Clamp, DDS 큐브맵 사용
  - 카메라 연동: RMB 회전 시 전체 방향에서 정상 표시, 위치는 무한원처럼 보이도록 처리(V의 평행이동 제거)
  - 참고: 큐브맵 레이아웃에 따라 `Fz`를 `Fx = diag(-1,1,1)`로 교체 가능

<p align="center">
  <img src="https://github.com/user-attachments/assets/298da7e9-6ee8-464e-aea9-188302c41fa1" width="60%" />
</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/88394728-2a5b-4d5f-b45a-29d763853a3f" width="60%" />
</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/e89561ca-9e31-403e-9641-c2095cb74f38" width="60%" />
</p>
