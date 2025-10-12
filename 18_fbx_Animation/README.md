## 18. fbx Animation (18_fbx_Animation)

- 내용 : 본 구조가 있는 캐릭터 fbx 파일에 내장되어 있는 애니메이션을 재생하는 예제입니다.
- 주요 구현
  - 본 구조를 정의합니다
  - 트랜스폼의 부모-자식 관계를 정의하고 각 트랜스폼에 맞게 SRT 적용, MVP를 적용합니다.
  - CPU에서 그리게 되면 매우 느려지기 때문에, 쉐이더에게 그리도록 해야합니다.
  - 버텍스가 매우 많은 모델도 그려낼 수 있도록 본 버퍼의 최대 개수를 1023개로 설정했습니다.


| fbx Animation - Phong  | fbx Animation - Blinn Phong  |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/e283f7a8-132c-4cf3-8474-6d9246e8e827" width="450"/>]()<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/bcc8cf4a-cd32-47b9-8dc7-1f4ec107e1d3" width="450"/>]()<br/></div> |


| fbx Animation - Lambert | fbx Animation - TextureOnly  |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/63eff4c7-23f3-4e52-a625-e121f9053681" width="450"/>]()<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/ae6029d7-f61f-43ee-b6a9-482eabad4a99" width="450"/>]()<br/></div> |


| fbx Animation - No Lighting |
|---|
| <div align="center"><img src="https://github.com/user-attachments/assets/2832ca17-4897-415b-84c4-c2d48107e1a1" width="600"/></div> |
