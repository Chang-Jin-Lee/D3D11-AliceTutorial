## 12. Blinn Phone (12_Lighting_BlinnPhong)
- 내용 : Blinn Phone 쉐이더를 사용한 라이팅 예제입니다
- 주요 구현
  - Material은 `ambient`, `diffuse`, `specular`, `reflect`를 가지고 있음
  - Ambient는 현재 가지고 있는 메테리얼의 ambient와 빛의 ambient를 곱해 구합니다
  - Diffuse에 normal과 light 두 벡터의 내적값을 반영함 (theta를 넣음)
  - Specular는 반사광과 시선벡터 사이의 중간 벡터로 구현
 

<img width="1282" height="1000" alt="스크린샷 2025-09-23 172053" src="https://github.com/user-attachments/assets/5fd06ccc-eb4d-47cc-98ce-be4beceaa176" />
<img width="1282" height="1000" alt="스크린샷 2025-09-23 165756" src="https://github.com/user-attachments/assets/fa898a4a-0d4f-459c-b0df-f93ad3fd7c0e" />
<img width="1282" height="1000" alt="스크린샷 2025-09-23 165738" src="https://github.com/user-attachments/assets/92ed6dcc-f59b-493c-875a-e4ef5d173cbb" />
<img width="1282" height="1000" alt="스크린샷 2025-09-23 165710" src="https://github.com/user-attachments/assets/243949f7-7e8d-4de6-9939-b292142f0110" />
