pmxCommon: Saba 스타일 PMX+VMD 파이프라인
- PmxTypes.h : 공통 타입(버텍스/본/스켈레톤)
- PmxMotion.* : VMD 로더/샘플러/이름정규화
- PmxSkinning.hlsl : 스키닝 정점 셰이더

App.cpp에서는:
- 스켈레톤 빌드 → 바인드 글로벌 → inverseBind(offset) 작성
- VMD 로드 → 프레임 샘플 → 노드 localAnim = bindLocal * (S*(T*R)*S)
- global = parent*local
- final = offset * global → BoneCB(b1) 업로드(전치) 