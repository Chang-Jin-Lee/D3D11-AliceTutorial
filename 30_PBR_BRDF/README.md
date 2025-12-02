# 30. PBR BRDF (Physically Based Rendering)

## PBR을 사용하는 이유

기존 Phong 셰이딩 렌더링에는, 다음과 같은 문제점이 있었습니다.

1. 비현실적인 반사가 있음: Phong은 단순히 코사인 값과 반사 강도로 스펙큘러를 계산합니다. 실제 물리 법칙과는 거리가 멀어서 금속, 플라스틱, 천 등 서로 다른 재질이 비슷하게 보이는 문제가 있었습니다. 물체의 표면이 미세하게 다르기 때문입니다.

2. 환경 변경의 어려움: Phong 셰이딩은 환경(조명, 주변)이 바뀔 때마다 머티리얼 파라미터를 다시 조정해야 했습니다. 한 환경에서 완벽하게 보이던 모델이 다른 환경에서는 이상하게 보였습니다.

 PBR은 실제 물리 법칙에 기반하여 빛과 재질의 상호작용을 계산합니다. 표면에 대해 메탈니스, 러프니스 파라미터를 사용합니다.

## PBR의 장단점

### 장점

1. 물리적으로 비교적 정확함. 실제 빛의 반사 법칙에 기반하여 계산하므로, 현실에 가까운 결과를 얻을 수 있습니다.

2. 한 번 설정한 머티리얼은 다양한 조명 환경에서도 일관되게 보입니다.

3. 파라미터
   - Metalness (0~1): 금속 재질인가 아닌가
   - Roughness (0~1): 표면이 거칠고 매끄러운가
   - 이 두 값만으로도 다양한 재질을 표현할 수 있습니다.

4. 반사되는 빛의 총합이 들어오는 빛의 총합을 넘지 않아 자연스럽습니다.

### 단점

1. 계산 비용: Phong보다 더 복잡한 수식을 사용하므로 GPU 연산이 더 필요합니다.

2. 환경광 필요: 사실적인 결과를 위해서는 IBL(Image-Based Lighting) 등 환경광이 필요합니다. (현재 구현은 단순한 Ambient로 되어 있습니다)

## 구현 방법

### 1. Cook-Torrance BRDF

PBR의 핵심은 Cook-Torrance 미세면 BRDF(Bidirectional Reflectance Distribution Function)입니다. 이는 다음 세 가지 요소로 구성됩니다:

```
BRDF = (D × G × F) / (4 × (N·V) × (N·L))
```
- D (Normal Distribution Function): 표면의 미세한 기복이 빛을 반사하는 방향을 결정합니다.
  - GGX/Trowbridge-Reitz 분포 사용
- G (Geometry Function): 미세한 기복에 의해 빛이 가려지는 현상을 계산합니다.
  - Smith 기하 함수 (Schlick-GGX 근사)
- F (Fresnel): 시야각에 따라 반사되는 빛의 양이 달라지는 현상을 계산합니다.
  - 여기서 Fresnel에 대한 Schlick 근사를 사용합니다.

### 2. 메탈니스-러프니스

- Base Color: 재질의 기본 색상
  - 금속: 표면의 반사 색상
  - 비금속: 알베도(산란 색상)
- Metalness (0~1): 금속 재질인 정도
- Roughness (0~1): 표면의 거칠기
- Ambient Occlusion: 그림자가 생기는 정도

### 3. 감마 보정 (Gamma Correction)

사람의 눈은 밝기를 선형적으로 느끼지 않고 로그 형태로 느낍니다. 따라서:

- 텍스처 로딩 시: sRGB 텍스처를 선형 공간으로 변환
  ```
  Linear = pow(sRGB, 2.2)
  ```
- 최종 출력 시: 선형 공간 색상을 sRGB로 변환
  ```
  sRGB = pow(Linear, 1/2.2)
  ```

이렇게 하면 사람의 눈에 자연스럽게 보입니다.

### 4. 구현 세부사항

#### 헬퍼 함수들

- `DistributionGGX()`: 미세면 분포 계산
- `GeometrySmith()`: 기하 함수 계산 (뷰와 라이트 모두 고려)
- `FresnelSchlick()`: 프레넬 효과 계산

#### 라이팅 계산

1. Diffuse BRDF: Lambertian diffuse (에너지 보존 고려)
   ```
   kD = (1 - F) × (1 - metalness)
   diffuse = kD × albedo / π
   ```

2. Specular BRDF: Cook-Torrance (D, G, F 사용)
   ```
   specular = (D × G × F) / (4 × (N·V) × (N·L))
   ```

3. 최종 색상: 
   ```
   color = (diffuse + specular) × radiance × (N·L) × AO
   color += albedo × ambient × AO
   color = pow(color, 1/gamma)  // 감마 인코딩
   ```

### 5. ImGui 조절 가능한 파라미터

- Base Color: 재질의 기본 색상
- Metalness (0~1): 금속 정도
- Roughness (0.04~1): 표면 거칠기
- Ambient Occlusion (0~1): 그림자 정도
- Gamma (1.4~10): 감마 보정 값 (기본값 2.2)

## 사용 방법

1. ImGui의 Shading Mode를 "PBR"로 선택합니다.
2. Material / PBR 섹션에서 각 모델별로 PBR 파라미터를 조절합니다.
3. 텍스처가 있는 경우, 텍스처의 G 채널(Roughness), B 채널(Metalness)도 사용됩니다.
4. 감마 값은 기본값(2.2)을 유지하는 것을 권장합니다. 조명이 어두울 때만 약간 조절합니다.

## 참고 자료

- Cook-Torrance BRDF 모델
- GGX/Trowbridge-Reitz 분포
- Smith 기하 함수
- Schlick 프레넬 근사
- sRGB 감마 보정

---