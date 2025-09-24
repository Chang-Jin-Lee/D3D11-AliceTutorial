## 14. LightingPhong (14_Lighting_Phong)
- 내용 : LightingPhong을 사용한 쉐이더 예제입니다.
- 주요 구현
  - LightingPhong 쉐이더를 사용합니다
  - Material의 Shiness인 g_Material.specular.w를 1까지 줄였을때 빛의 반대 방향에 Specular가 적용되는 문제
  - normal 벡터와 light 벡터를 내적한 결과 NdotL과
  - normal 벡터와 eye 벡터를 내적한 결과 NdotV
  - 이 둘을 곱한 결과가 0보다 클때를 나타내는 변수를 만듭니다.
    - specGate = saturate(sign(theta)) * saturate(sign(NdotV));
  - 이 specGate를 specularScalar에 곱합니다.

- 참고 사이트 
https://dicklyon.com/tech/Graphics/Phong_TR-Lyon.pdf

https://google.github.io/filament/Filament.md.html

https://graphics.pixar.com/library/ReflectanceModel/paper.pdf

https://discussions.unity.com/t/bug-in-all-specular-shaders-causing-highlights-from-light-sources-behind-objects/432906/4

<img width="2052" height="1600" alt="image" src="https://github.com/user-attachments/assets/6ab653c5-7e86-436f-835e-bbd6f536e59c" />