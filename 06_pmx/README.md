## 06. PMX (A-Pose)
- 내용: PMX 캐릭터를 A-포즈 실루엣(흰색)으로 단순 렌더링
- 주요 구현:
  - Assimp로 PMX 로드, 노드 계층(Global Transform) 적용 병합
  - AABB 기반 중심 이동/스케일 정규화, 카메라 자동 설정
  - 텍스처/머티리얼 생략(픽셀 셰이더 고정 마젠타색)
  - Rasterizer Cull None, Depth 테스트 활성화
  - ImGui로 루트 위치/카메라(FOV/Near/Far) 조정
- 결과: 화면에 PMX A-포즈 캐릭터 실루엣 표시
  
<p align="center">
  <img src="https://github.com/user-attachments/assets/e2de8438-8e10-4c28-a28b-ed25736a5756" width="60%" />
</p>
