## 17. fbx_pmx_obj (17_fbx_pmx_obj_WithPhong)

- 내용 : Phong Shading을 사용하고 fbx, pmx, obj 모델을 로드하여 렌더링하는 프로젝트입니다.
- 주요 구현
  - assimp에서 모델 파일 안에 텍스처가 있는지 확인합니다. 만약 있다면 그 텍스처를 사용합니다.
  - 없다면 모델에 정의되어 있는 텍스처 경로를 탐색합니다.
  - 메시는 scene->mMeshes[node->mMeshes[mi]]; 에 접근해서 메시 데이터를 가져옵니다
  - TBN의 Tangent값은 assimp 안에 있는 mTangents 값을 사용합니다
  - TBN의 Bitangents값은 assimp 안에 있는 mBitangents값을 사용합니다
  - TBN의 Normal값은 assimp 안에 있는 mNormals값을 사용합니다
  - 각 노드의 position 데이터는 mVertices에서 가져옵니다.
  - 마지막으로 쉐이더에게 값을 전달하면 합니다.

## PMX

| pmx - Phong  | pmx - Blinn Phong  |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/0e9f5864-6690-4464-b60b-b7e3c20a136f" width="450"/>]()<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/f4053efc-2f1d-4e42-be28-f597521886fe" width="450"/>]()<br/></div> |

| pmx - Lambert | pmx - TextureOnly  |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/50bf6e62-b9ba-421c-8e69-8df151ce19de" width="450"/>]()<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/925c3ca6-533c-420b-ba8b-b626fc0cbd03" width="450"/>]()<br/></div> |


## FBX

| fbx - Phong  | fbx - Blinn Phong  |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/8085cef0-9c9a-426d-ad04-a9c36e949ad5" width="450"/>]()<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/67be9ca5-4142-401f-b533-64b23938724d" width="450"/>]()<br/></div> |

| fbx - Lambert | fbx - TextureOnly  |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/431864f2-41c1-4bc9-b8c5-29325d976af4" width="450"/>]()<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/18637cb0-e8d6-4a04-aad2-9b559224b0f5" width="450"/>]()<br/></div> |


| fbx - Phong  | fbx - Blinn Phong  |
|--------------|--------------------|
| <div align="center">[<img src="https://github.com/user-attachments/assets/54197bf9-15c8-4488-b934-0b3866d3a355" width="450"/>]()<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/84ffebbc-20db-4cb0-8614-6bab06dbaa92" width="450"/>]()<br/></div> |

| fbx - Lambert | fbx - TextureOnly  |
|--------------|-------------------|
| <div align="center">[<img src="https://github.com/user-attachments/assets/54a7d0f5-4ca6-46a9-a2e3-a72effdea8a6" width="450"/>]()<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/c7883b9b-2586-445f-9082-e951a734993d" width="450"/>]()<br/></div> |



## OBJ

| obj - Phong  | obj - Blinn Phong  |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/eeb78589-9bab-4373-9248-bcaccafe3a49" width="450"/>]()<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/d4fda6ef-aec6-4fc9-8532-4dc39e38c8c1" width="450"/>]()<br/></div> |

| obj - Lambert | obj - TextureOnly  |
|---|---|
| <div align="center">[<img src="https://github.com/user-attachments/assets/43a7690e-a3b8-44d8-a9e8-d6dca84f525b" width="450"/>]()<br/></div> | <div align="center">[<img src="https://github.com/user-attachments/assets/35520c47-e2b6-40f8-b207-132c0b8a6fd8" width="450"/>]()<br/></div> |

