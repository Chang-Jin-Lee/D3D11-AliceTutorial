
## 01. RenderingQuadangle
- 내용: NDC 좌표계를 기반으로 두 개의 삼각형을 그려 사각형을 렌더링  
- 주요 구현:
  - Vertex / Index Buffer 생성
  - Input Layout 및 Shader 연결
  - `IASetPrimitiveTopology` 를 이용한 삼각형 리스트 설정
- 결과: 화면 중앙에 사각형 출력

<p align="center">
  <img src="https://github.com/user-attachments/assets/a44c63b4-0313-4c7d-b98f-03bfcf7abaa0" width="60%" />
</p>

---