## 14. LightingPhong (14_Lighting_Phong)


- 이미지를 클릭하면 이동합니다

| [유튜브](https://www.youtube.com/watch?v=k1ex_n8N5AU) | [블로그](https://velog.io/@whoamicj/DX11-14LightingPhong-Phong-%EC%85%B0%EC%9D%B4%EB%94%A9-%EC%95%8C%ED%8C%8C-%ED%88%AC%EB%AA%85%EC%B2%98%EB%A6%AC-%EB%9F%AC%ED%94%84%EB%8B%88%EC%8A%A4) |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/b704e2dc-a131-4154-817c-c168174202cd" width="450"/>](https://www.youtube.com/watch?v=k1ex_n8N5AU)<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/ed99ceaf-7235-4a8e-a968-a2c55c6254ac" width="230"/>](https://velog.io/@whoamicj/DX11-14LightingPhong-Phong-%EC%85%B0%EC%9D%B4%EB%94%A9-%EC%95%8C%ED%8C%8C-%ED%88%AC%EB%AA%85%EC%B2%98%EB%A6%AC-%EB%9F%AC%ED%94%84%EB%8B%88%EC%8A%A4)<br/></div> |


- 내용 : LightingPhong을 사용한 쉐이더 예제입니다.
- 주요 구현
  - LightingPhong 쉐이더를 사용합니다
  - Material의 Shiness인 g_Material.specular.w를 1까지 줄였을때 빛의 반대 방향에 Specular가 적용되는 문제
  - normal 벡터와 light 벡터를 내적한 결과 NdotL과
  - normal 벡터와 eye 벡터를 내적한 결과 NdotV
  - 이 둘을 곱한 결과가 0보다 클때를 나타내는 변수를 만듭니다.
    - specGate = saturate(sign(theta)) * saturate(sign(NdotV));
  - 이 specGate를 specularScalar에 곱합니다.


  - 투명은 다음처럼 텍스쳐 컬러의 알파값에 메테리얼의 난반사 알파 값을 곱해서 만들어 둡니다.
  - 마지막 컬러 값의 알파에 덮어쓰기 하면 됩니다
  ```
  float alphaTex = textureColor.a * g_Material.diffuse.a;
  clip(alphaTex - 0.1f);
  ......
  litColor.a = alphaTex;
  ```

- 참고 사이트

  
https://dicklyon.com/tech/Graphics/Phong_TR-Lyon.pdf

https://google.github.io/filament/Filament.md.html

https://graphics.pixar.com/library/ReflectanceModel/paper.pdf

https://discussions.unity.com/t/bug-in-all-specular-shaders-causing-highlights-from-light-sources-behind-objects/432906/4

<img width="2052" height="1600" alt="image" src="https://github.com/user-attachments/assets/51aff0cf-a20d-42ae-86e1-d49b701f5b88" />


<img width="2052" height="1600" alt="스크린샷 2025-09-24 221313" src="https://github.com/user-attachments/assets/5b5b3dd6-9d38-4f30-beaa-d2ef76dea31b" />


<img width="2052" height="1600" alt="스크린샷 2025-09-24 221320" src="https://github.com/user-attachments/assets/9468b4b8-ee7d-4920-bdc0-91ffbbbc6b72" />
