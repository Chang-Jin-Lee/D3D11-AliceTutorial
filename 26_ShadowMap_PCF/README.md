## 26. ShadowMap PCF (26_ShadowMap_PCF)

- 내용 : ShadowMap을 보여주는 예제 입니다.
- 주요 구현
  - 깊이 텍스쳐 (DSV, SRV 겸용인거)를 생성합니다
  - Output Merger에 DSV만 바인딩 합니다. RTV는 none으로 둡니다
  - vertex shader에서 월드 -> 라이팅의 뷰 - 프로젝션 변환을 하고 깊이를 기록합니다
  - 메인 패스(제 프로젝트에서는 쉐이더 코드들이 모아져 있는 부분)에서 t4에 Shadow 맵 SRV와 s1와 샘플러를 바인딩합니다.
  - pixel shader에서 라이트를 공간 좌표로 Shadow map을 샘플링하고 상수버퍼를 통해 값들은 전달합니다

| Skinned With Bone Structure |
|---|
| <div align="center"><img src="" width="600"/></div> |
