# 36. Animation, FMOD 3D Sound, Multithread

이 이후의 애니메이션 구현은 다음의 레포에서 계속됩니다

https://github.com/Chang-Jin-Lee/D3D11-AliceAnimation

### 애니메이션
- Animation Blend
- Additive
- Animation Layer
- IK
- Socket



| [1. 앞 워크](https://github.com/user-attachments/assets/0d733289-754b-4aa6-9c83-ea4d872e4852) | [2. 앉기 일어서기 오른쪽](https://github.com/user-attachments/assets/6caebd2c-38f6-483b-957d-d9b7982b6602) |
|---|---|
| <div align="center"><video src="https://github.com/user-attachments/assets/0d733289-754b-4aa6-9c83-ea4d872e4852" width="400" muted loop autoplay controls>지원 안됨</video></div> | <div align="center"><video src="https://github.com/user-attachments/assets/6caebd2c-38f6-483b-957d-d9b7982b6602" width="400" muted loop autoplay controls>지원 안됨</video></div><br>앉기, 일어서기 오른쪽에서 본 애니 |

| [3. 왼쪽→오른쪽 턴 런](https://github.com/user-attachments/assets/2fc33df1-9a10-42ac-bd3f-c4b1f44e1059) | [4. 왼쪽 런](https://github.com/user-attachments/assets/566b29b0-10e8-495a-a330-06d9a7f13fc2) |
|---|---|
| <div align="center"><video src="https://github.com/user-attachments/assets/2fc33df1-9a10-42ac-bd3f-c4b1f44e1059" width="400" muted loop autoplay controls>지원 안됨</video></div><br>왼쪽을 보고 있다가 오른쪽으로 돌아서서 뛰는 애니 | <div align="center"><video src="https://github.com/user-attachments/assets/566b29b0-10e8-495a-a330-06d9a7f13fc2" width="400" muted loop autoplay controls>지원 안됨</video></div><br>왼쪽을 보고 있다가 왼쪽으로 뛰는 애니 |

| [5. 왼쪽 워크](https://github.com/user-attachments/assets/6007cca8-9932-4df1-a4a9-e23b6d253ac9) | [6. 오른쪽 워크](https://github.com/user-attachments/assets/00dbd667-1e74-4214-9a30-a29d0e846eee) |
|---|---|
| <div align="center"><video src="https://github.com/user-attachments/assets/6007cca8-9932-4df1-a4a9-e23b6d253ac9" width="400" muted loop autoplay controls>지원 안됨</video></div><br>왼쪽으로 걷는 애니 | <div align="center"><video src="https://github.com/user-attachments/assets/00dbd667-1e74-4214-9a30-a29d0e846eee" width="400" muted loop autoplay controls>지원 안됨</video></div><br>오른쪽으로 걷는 애니 |

| [7. 앉기→일어서기](https://github.com/user-attachments/assets/79219122-6dd8-400a-afee-1c75470e4fc3) | [8. 재장전](https://github.com/user-attachments/assets/45f5d9fd-5e52-42c6-a184-a873913a61fe) |
|---|---|
| <div align="center"><video src="https://github.com/user-attachments/assets/79219122-6dd8-400a-afee-1c75470e4fc3" width="400" muted loop autoplay controls>지원 안됨</video></div><br>앉아있다가 일어서는 애니 | <div align="center"><video src="https://github.com/user-attachments/assets/45f5d9fd-5e52-42c6-a184-a873913a61fe" width="400" muted loop autoplay controls>지원 안됨</video></div><br>재장전 애니 |

| [10. 앉기+슈팅](https://github.com/user-attachments/assets/18f5d4bf-6a92-4d4f-8f0b-dac94948568a) | [11. 앉기+일어서기](https://github.com/user-attachments/assets/531902df-a9f7-4e4b-8539-0efab43484d1) |
|---|---|
| <div align="center"><video src="https://github.com/user-attachments/assets/18f5d4bf-6a92-4d4f-8f0b-dac94948568a" width="400" muted loop autoplay controls>지원 안됨</video></div><br>앉는 애니 + 총 쏘는 애니 | <div align="center"><video src="https://github.com/user-attachments/assets/531902df-a9f7-4e4b-8539-0efab43484d1" width="400" muted loop autoplay controls>지원 안됨</video></div><br>앉는 애니 + 일어서는 애니 |

| [12. 앞 워크/런](https://github.com/user-attachments/assets/8c815f23-c69a-4363-a526-a726b81069b8) | [13. 뒤 워크/런](https://github.com/user-attachments/assets/cb2c9c08-d258-41ee-ae01-d65e64922b0c) |
|---|---|
| <div align="center"><video src="https://github.com/user-attachments/assets/8c815f23-c69a-4363-a526-a726b81069b8" width="400" muted loop autoplay controls>지원 안됨</video></div><br>걷고, 뛰는 애니 앞모습 | <div align="center"><video src="https://github.com/user-attachments/assets/cb2c9c08-d258-41ee-ae01-d65e64922b0c" width="400" muted loop autoplay controls>지원 안됨</video></div><br>걷고, 뛰는 애니 뒷모습 |

| [14. 뒤 워크](https://github.com/user-attachments/assets/edd19e6a-40cb-421d-92a9-9efdfe2ae3a1) | [15. 앞 워크](https://github.com/user-attachments/assets/16bc7491-8f86-4e9e-af52-e24231359288) |
|---|---|
| <div align="center"><video src="https://github.com/user-attachments/assets/edd19e6a-40cb-421d-92a9-9efdfe2ae3a1" width="400" muted loop autoplay controls>지원 안됨</video></div><br>걷는 애니 뒷모습 | <div align="center"><video src="https://github.com/user-attachments/assets/16bc7491-8f86-4e9e-af52-e24231359288" width="400" muted loop autoplay controls>지원 안됨</video></div><br>걷는 애니 앞모습 |
