## 25. ToonShading Outline (25_ToonShading_Outline)

- 내용 : ToonShading과 Outline을 보여주는 예제입니다. 이전 쉐이더들과 한눈에 비교할 수 있게 배치했습니다.
- 주요 구현
  - Diffuse에서 Theta를 나누어서 계단식으로 그리게끔 만들었습니다.
  - Specular에서 N Dot H가 1에 가까운 구간을 강조하게 했습니다
  - 본을 그리는 패스를 렌더한 뒤, 멀티 패스에서 정점을 뷰-공간 XY로 바깥쪽으로 키웁니다.
  - 이후에 “백페이스”만 렌더합니다. 깊이는 읽기만 해서 실루엣만 남기고 앞면 색은 보이지 않게 합니다.
  - 프로젝트가 렉이 걸린다면 App::OnInitialize() 함수 내부의 fbx 파일 로드와 밑의 모델들 데이터 수정 하는 부분을 삭제하면 됩니다.

| All Shader Collection |
|---|
| <div align="center"><img src="https://github.com/user-attachments/assets/c637fcf1-d720-4966-8c09-d3f3f1e854e0" width="1600"/></div> |


| Unlit | Lambert  | BlinnPhong |  
|---|---|---|
| <div align="center"><img src="https://github.com/user-attachments/assets/60c8d563-d164-4e6d-ac4f-10cdc2d9d9b6" width="450"/></div> | <div align="center"><img src="https://github.com/user-attachments/assets/0b61f6af-835d-447e-afe0-58df6e68f563" width="450"/></div> | <div align="center"><img src="https://github.com/user-attachments/assets/a52fed7c-e574-4ad7-bf69-bc065a4dbdf9" width="450"/></div> |

Phong | TextureOnly | ToonShading | ToonShading + outline |
|---|---|---|---|
|  <div align="center"><img src="https://github.com/user-attachments/assets/aefd4ddc-2b7e-40d2-95b6-76bbbf64b233" width="550"/></div> | <div align="center"><img src="https://github.com/user-attachments/assets/1282063c-108d-4ffb-91cd-f583f4ae0372" width="450"/></div> | <div align="center"><img src="https://github.com/user-attachments/assets/dec73a6c-5b84-40e5-b618-cf24a98fe7ed" width="450"/></div> | <div align="center"><img src="https://github.com/user-attachments/assets/19117156-fa9a-4be0-b2e0-633e6ce47c24" width="450"/></div> |
