/*
 * @brief  : ToneMapping
 * @details: 톤 매핑, HDR 렌더링, 감마 보정 등을 구현한 데모입니다.
 */

#include "App.h"
#include "../Common/AssetManager.h"
#include "../Common/BaseObject.h"
#include "../Common/Helper.h"
#include "../Common/LineRenderer.h"
#include "../Common/Mesh/FbxAnimation.h"
#include "../Common/Mesh/FbxModel.h"
#include "../Common/ObjManager.h"
#include "../Common/PmxManager.h"
#include "../Common/Ray.h"
#include "../Common/Skybox.h"
#include "../Common/SkyboxAssetManager.h"
#include "../Common/StaticMesh.h"
#include "../Common/SystemInfomation.h"
#include "../Common/Transform.h"
#include "../Common/mmd/VmdCameraPlayer.h"
#include "../Common/Sound/SoundSystem.h"
#include "../Common/Sound/SoundManager.h"
#include "SceneA.h"
#include "SceneB.h"
#include <algorithm>
#include <format>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cfloat>
#include <cmath>
#include <cctype>
#include <commdlg.h>
#include <cstring>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxtk/DDSTextureLoader.h>
#include <directxtk/GamePad.h>
#include <directxtk/SimpleMath.h>
#include <directxtk/WICTextureLoader.h>
#include <dxgi1_4.h>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <memory>
#include <random>
#include <thread>
#include <unordered_set>
#include <windows.h>
#include <wrl/client.h>


#include <dxgi1_4.h> // swapchain3 ToneMapping을 위한 것
#include <dxgi1_6.h> // swapchain3 ToneMapping을 위한 것
#include "../Common/Animation/Animator.h"
#include "../Common/Animation/CharacterAnimController.h"
#include <array>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "Comdlg32.lib")

using namespace DirectX;
using namespace DirectX::SimpleMath;


// Implementation is split by responsibility for portfolio readability.
// These files are included here so App::Impl and file-local helpers remain private to this translation unit.
#include "App_InternalTypes.inl"
#include "App_Utilities.inl"
#include "App_PublicDemoRuntime.inl"
#include "App_Lifecycle.inl"
#include "App_UpdateInput.inl"
#include "App_RenderPasses.inl"
#include "App_ModelLoading.inl"
#include "App_ImGuiPanels.inl"
