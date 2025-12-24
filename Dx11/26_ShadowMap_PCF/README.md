## 26. ShadowMap PCF (26_ShadowMap_PCF)

- 내용 : ShadowMap을 보여주는 예제 입니다.
- 주요 구현
  - 깊이 텍스쳐 (DSV, SRV 겸용임)를 생성합니다
  - Output Merger에 DSV만 바인딩 합니다. RTV는 none으로 둡니다
  - vertex shader에서 월드 -> 라이팅의 뷰 - 프로젝션 변환을 하고 깊이를 기록합니다
  - 메인 패스(제 프로젝트에서는 쉐이더 코드들이 모아져 있는 부분)에서 t4에 Shadow 맵 SRV와 s1와 샘플러를 바인딩합니다.
  - pixel shader에서 라이트를 공간 좌표로 Shadow map을 샘플링하고 상수버퍼를 통해 값들은 전달합니다
  
- 주의할 점
  - 기준 점을 잘 찾아야 합니다. 모델의 원점에서 보통 하게 되는데, 이때 uv 좌표를 잘못 설정하면 빛 방향으로 그림자가 나오게 됩니다. 그림자가 반대로 생긴다는 이야기입니다

| 그림자가 반대로 생긴 사진 |
|---|
| <div align="center"><img src="https://github.com/user-attachments/assets/629d1a4d-f10b-453d-8a81-d64b43b87085" width="600"/></div> |


</br>

| 그림자가 제대로 그려진 사진 | 그림자가 제대로 그려진 사진2 | 
|---|---|
| <div align="center"><img src="https://github.com/user-attachments/assets/4bde0d75-f15a-4c94-81f2-23c14c405384" width="600"/></div> | <div align="center"><img src="https://github.com/user-attachments/assets/c0d19bec-bd8c-4f36-bc7f-7a85b063034d" width="600"/></div> | 
