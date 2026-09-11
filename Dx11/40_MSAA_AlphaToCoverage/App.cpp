#include "App.h"
#include "AppFailure.h"

#include "../Common/Animation/Animator.h"
#include "../Common/Mesh/FbxModel.h"
#include "../Common/ReadmeCapture.h"
#include "../Common/Vertex.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
constexpr float NearPlane = 0.05f;
constexpr float FarPlane = 12.0f;

static_assert(sizeof(VertexSkinnedTBN) == 96);
static_assert(offsetof(VertexSkinnedTBN, boneIdx) == 72);
static_assert(offsetof(VertexSkinnedTBN, boneWeight) == 80);

HRESULT CompileSurface(const char* entry, const char* target, ComPtr<ID3DBlob>& bytecode)
{
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    const HRESULT result = D3DCompileFromFile(L"40_Surface.hlsl", nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target, flags, 0,
        bytecode.ReleaseAndGetAddressOf(), errors.GetAddressOf());
    if (errors)
        OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
    return result;
}

template<typename T>
bool ReadBuffer(ID3D11Device* device, ID3D11DeviceContext* context,
    ID3D11Buffer* source, std::vector<T>& result)
{
    if (!source)
        return false;
    D3D11_BUFFER_DESC description{};
    source->GetDesc(&description);
    const UINT byteWidth = description.ByteWidth;
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;
    description.StructureByteStride = 0;
    ComPtr<ID3D11Buffer> staging;
    if (FAILED(device->CreateBuffer(&description, nullptr, &staging)))
        return false;
    context->CopyResource(staging.Get(), source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        return false;
    result.resize(byteWidth / sizeof(T));
    std::memcpy(result.data(), mapped.pData, result.size() * sizeof(T));
    context->Unmap(staging.Get(), 0);
    return true;
}

bool IsSrgbFormat(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}

std::string Narrow(const std::wstring& value)
{
    if (value.empty())
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return "unavailable reason could not be converted";
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

float SceneWidth(UINT clientWidth)
{
    return std::max(1.0f, static_cast<float>(clientWidth) -
        std::min(390.0f, clientWidth * 0.38f));
}
}

namespace Coverage40
{
App::App()
{
    wcscpy_s(m_szTitle, L"40. MSAA Alpha-to-Coverage - Alice Tutorial");
    wcscpy_s(m_szWindowClass, L"AliceCoverage40");
    XMStoreFloat4x4(&characterWorld_, XMMatrixRotationY(-0.15f));
}

App::~App()
{
    OnUninitialize();
}

bool App::Fail(const wchar_t* operation, HRESULT hr)
{
    if (!failed_)
    {
        wchar_t message[768]{};
        swprintf_s(message, L"%s\nHRESULT: 0x%08X", operation,
            static_cast<unsigned>(hr));
        OutputDebugStringW(message);
        MessageBoxW(m_hWnd, message, L"40 MSAA Alpha-to-Coverage",
            MB_OK | MB_ICONERROR);
    }
    failed_ = true;
    return false;
}

bool App::OnInitialize()
{
    capture_ = ReadmeCapture::IsEnabled();
    orbit_ = false;
    settings_ = {};
    settings_.mode = Mode::AlphaTest1x;
    settings_.alphaMultiplier = 1.0f;
    exposure_ = 1.0f;
    scene_ = Scene::Character;
    closeUp_ = true;

    if (!CreateDevice())
        return false;
    if (!pipeline_.Initialize(device_.Get(), L"."))
    {
        OutputDebugStringW(pipeline_.Support().reason.c_str());
        return Fail(L"MsaaPipeline::Initialize / 40_Present.hlsl", E_FAIL);
    }
    if (!CreateSurfaceResources())
        return false;
    if (!LoadCharacter())
        return Fail(L"LoadCharacter / SampleModel.glb / VRM_1 at 0.5 s", E_FAIL);
    if (!Resize(m_ClientWidth, m_ClientHeight))
        return Fail(L"Initial backbuffer and MSAA target configuration", configurationHr_);

    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext())
        return Fail(L"ImGui::CreateContext", E_FAIL);
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplWin32_Init(m_hWnd))
    {
        ImGui::DestroyContext();
        return Fail(L"ImGui_ImplWin32_Init", E_FAIL);
    }
    if (!ImGui_ImplDX11_Init(device_.Get(), context_.Get()))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return Fail(L"ImGui_ImplDX11_Init", E_FAIL);
    }
    imgui_ = true;
    return true;
}

bool App::CreateDevice()
{
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount = 2;
    description.BufferDesc.Width = m_ClientWidth;
    description.BufferDesc.Height = m_ClientHeight;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = m_hWnd;
    description.SampleDesc.Count = 1;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
#if defined(_DEBUG)
    flags = D3D11_CREATE_DEVICE_DEBUG;
#endif
    const D3D_FEATURE_LEVEL requestedLevel = D3D_FEATURE_LEVEL_11_0;
    auto create = [&]() {
        return D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
            nullptr, flags, &requestedLevel, 1, D3D11_SDK_VERSION, &description,
            swapChain_.ReleaseAndGetAddressOf(), device_.ReleaseAndGetAddressOf(),
            nullptr, context_.ReleaseAndGetAddressOf());
    };
    HRESULT result = create();
    if (result == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
        flags = 0;
        result = create();
    }
    return SUCCEEDED(result) || Fail(L"D3D11CreateDeviceAndSwapChain", result);
}

bool App::CreateSurfaceResources()
{
    ComPtr<ID3DBlob> bytecode;
    HRESULT result = CompileSurface("VSMain", "vs_5_0", bytecode);
    if (FAILED(result))
        return Fail(L"D3DCompileFromFile(40_Surface.hlsl, VSMain)", result);
    result = device_->CreateVertexShader(bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(), nullptr, &surfaceVs_);
    if (FAILED(result))
        return Fail(L"CreateVertexShader(40_Surface VSMain)", result);

    const D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VertexSkinnedTBN,pos),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VertexSkinnedTBN,n),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(VertexSkinnedTBN,color),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(VertexSkinnedTBN,uv),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BLENDINDICES",0,DXGI_FORMAT_R16G16B16A16_UINT,0,offsetof(VertexSkinnedTBN,boneIdx),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BLENDWEIGHT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(VertexSkinnedTBN,boneWeight),D3D11_INPUT_PER_VERTEX_DATA,0}
    };
    result = device_->CreateInputLayout(elements, static_cast<UINT>(std::size(elements)),
        bytecode->GetBufferPointer(), bytecode->GetBufferSize(), &layout_);
    if (FAILED(result))
        return Fail(L"CreateInputLayout(96-byte VertexSkinnedTBN)", result);

    result = CompileSurface("PSMain", "ps_5_0", bytecode);
    if (FAILED(result))
        return Fail(L"D3DCompileFromFile(40_Surface.hlsl, PSMain)", result);
    result = device_->CreatePixelShader(bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(), nullptr, &surfacePs_);
    if (FAILED(result))
        return Fail(L"CreatePixelShader(40_Surface PSMain)", result);

    D3D11_BUFFER_DESC bufferDescription{};
    bufferDescription.ByteWidth = sizeof(SurfaceConstants);
    bufferDescription.Usage = D3D11_USAGE_DEFAULT;
    bufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    result = device_->CreateBuffer(&bufferDescription, nullptr, &constants_);
    if (FAILED(result))
        return Fail(L"CreateBuffer(Surface constants)", result);

    std::array<VertexSkinnedTBN, 4> vertices{};
    vertices[0].pos = { -0.5f, -0.5f, 0.0f }; vertices[0].uv = { 0.0f, 1.0f };
    vertices[1].pos = { -0.5f,  0.5f, 0.0f }; vertices[1].uv = { 0.0f, 0.0f };
    vertices[2].pos = {  0.5f,  0.5f, 0.0f }; vertices[2].uv = { 1.0f, 0.0f };
    vertices[3].pos = {  0.5f, -0.5f, 0.0f }; vertices[3].uv = { 1.0f, 1.0f };
    for (VertexSkinnedTBN& vertex : vertices)
    {
        vertex.n = { 0.0f, 0.0f, -1.0f };
        vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    }
    const UINT indices[]{ 0, 1, 2, 0, 2, 3 };
    D3D11_SUBRESOURCE_DATA initialData{ vertices.data(), 0, 0 };
    bufferDescription.ByteWidth = sizeof(vertices);
    bufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
    bufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    result = device_->CreateBuffer(&bufferDescription, &initialData, &diagnosticVertices_);
    if (FAILED(result))
        return Fail(L"CreateBuffer(diagnostic vertices)", result);
    bufferDescription.ByteWidth = sizeof(indices);
    bufferDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initialData.pSysMem = indices;
    result = device_->CreateBuffer(&bufferDescription, &initialData, &diagnosticIndices_);
    if (FAILED(result))
        return Fail(L"CreateBuffer(diagnostic indices)", result);

    std::vector<std::uint8_t> pixels(256U * 256U * 4U);
    for (std::uint32_t y = 0; y < 256; ++y)
    {
        for (std::uint32_t x = 0; x < 256; ++x)
        {
            const float u = float(x) / 255.0f;
            const float stripe = ((x + y) % 16 < 8) ? 1.0f : 0.0f;
            const float alpha = y < 128 ? u : stripe;
            const auto byteAlpha = static_cast<std::uint8_t>(std::lround(alpha * 255.0f));
            const std::size_t offset = (std::size_t(y) * 256 + x) * 4;
            pixels[offset + 0] = 255;
            pixels[offset + 1] = 255;
            pixels[offset + 2] = 255;
            pixels[offset + 3] = byteAlpha;
        }
    }
    D3D11_TEXTURE2D_DESC textureDescription{};
    textureDescription.Width = 256;
    textureDescription.Height = 256;
    textureDescription.MipLevels = 1;
    textureDescription.ArraySize = 1;
    textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDescription.SampleDesc.Count = 1;
    textureDescription.Usage = D3D11_USAGE_IMMUTABLE;
    textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA textureData{ pixels.data(), 256U * 4U, 0 };
    ComPtr<ID3D11Texture2D> patternTexture;
    result = device_->CreateTexture2D(&textureDescription, &textureData, &patternTexture);
    if (FAILED(result))
        return Fail(L"CreateTexture2D(256x256 diagnostic alpha pattern)", result);
    result = device_->CreateShaderResourceView(patternTexture.Get(), nullptr,
        &diagnosticPattern_);
    if (FAILED(result))
        return Fail(L"CreateShaderResourceView(diagnostic alpha pattern)", result);

    D3D11_RASTERIZER_DESC rasterizerDescription{};
    rasterizerDescription.FillMode = D3D11_FILL_SOLID;
    rasterizerDescription.CullMode = D3D11_CULL_BACK;
    rasterizerDescription.DepthClipEnable = TRUE;
    rasterizerDescription.MultisampleEnable = TRUE;
    result = device_->CreateRasterizerState(&rasterizerDescription, &cullBack_);
    if (FAILED(result))
        return Fail(L"CreateRasterizerState(cull back, multisample)", result);
    rasterizerDescription.CullMode = D3D11_CULL_NONE;
    result = device_->CreateRasterizerState(&rasterizerDescription, &cullNone_);
    if (FAILED(result))
        return Fail(L"CreateRasterizerState(cull none, multisample)", result);

    D3D11_SAMPLER_DESC samplerDescription{};
    samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
    result = device_->CreateSamplerState(&samplerDescription, &sampler_);
    if (FAILED(result))
        return Fail(L"CreateSamplerState(linear clamp)", result);

    auto addSurface = [&](float width, float height, float roll, XMFLOAT3 center,
        XMFLOAT4 color, bool pattern, bool doubleSided) {
        DiagnosticSurface surface{};
        XMStoreFloat4x4(&surface.world, XMMatrixScaling(width, height, 1.0f) *
            XMMatrixRotationZ(roll) * XMMatrixTranslation(center.x, center.y, center.z));
        surface.color = color;
        surface.pattern = pattern;
        surface.doubleSided = doubleSided;
        diagnosticSurfaces_.push_back(surface);
    };
    // Separate draws expose an opaque diagonal geometry edge, internal alpha boundaries,
    // and a foreground depth occluder without changing texture mip policy.
    addSurface(1.35f, 0.78f, 0.48f, { -0.48f, 0.28f, 0.34f },
        { 0.18f, 0.35f, 0.82f, 1.0f }, false, false);
    addSurface(1.12f, 1.18f, 0.0f, { 0.42f, 0.22f, 0.16f },
        { 0.15f, 0.78f, 0.54f, 1.0f }, true, true);
    addSurface(0.24f, 0.72f, -0.18f, { 0.34f, 0.02f, -0.06f },
        { 0.82f, 0.60f, 0.16f, 1.0f }, false, false);
    return true;
}

bool App::LoadCharacter()
{
    character_ = std::make_unique<FbxModel>();
    if (!character_->Load(device_.Get(),
            L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb") ||
        !character_->HasMesh() ||
        character_->GetVertexStride() != sizeof(VertexSkinnedTBN) ||
        !character_->GetBoneConstantBuffer())
        return false;

    const auto& names = character_->GetAnimationNames();
    int selected = -1;
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        std::string name = names[index];
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (name == "vrm_1")
        {
            selected = static_cast<int>(index);
            break;
        }
    }
    if (selected < 0)
        return false;

    character_->SetCurrentAnimation(selected);
    character_->SetAnimationPlaying(false);
    character_->SetAnimationTimeSeconds(0.5);
    poseName_ = names[static_cast<std::size_t>(selected)];

    const aiScene* scene = character_->GetScenePtr();
    if (!scene || static_cast<unsigned>(selected) >= scene->mNumAnimations)
        return false;
    CharacterAnimator pose;
    pose.Initialize(device_.Get(), scene, character_->GetNodeIndexOfName(),
        character_->GetGlobalInverse(), character_->GetBoneNames(),
        character_->GetBoneOffsets());
    if (!pose.GetBoneCB())
        return false;
    const aiAnimation* clip = scene->mAnimations[selected];
    pose.UpdateAnimation(0.0f, clip, 0.5f, clip, 0.5f, 0.0f);
    pose.UploadPalette(context_.Get(), pose.finalTransforms);
    context_->CopyResource(character_->GetBoneConstantBuffer(), pose.GetBoneCB());
    if (!BuildPosedCentroids())
        return false;

    const auto& alpha = character_->GetMaterialAlphaInfos();
    const auto& textures = character_->GetMaterialSRVs();
    if (alpha.size() != scene->mNumMaterials)
        return false;
    materials_.resize(scene->mNumMaterials);
    for (std::size_t index = 0; index < materials_.size(); ++index)
    {
        Material& material = materials_[index];
        aiString name;
        scene->mMaterials[index]->Get(AI_MATKEY_NAME, name);
        material.name = name.C_Str();
        material.mode = static_cast<AlphaMode>(alpha[index].mode);
        material.cutoff = alpha[index].cutoff;

        aiColor4D factor(1.0f, 1.0f, 1.0f, 1.0f);
        // FbxMaterial loads the texture bytes only; the linear base-color factor remains separate.
        scene->mMaterials[index]->Get(AI_MATKEY_BASE_COLOR, factor);
        material.factor = { factor.r, factor.g, factor.b, factor.a };
        int twoSided = 0;
        scene->mMaterials[index]->Get(AI_MATKEY_TWOSIDED, twoSided);
        material.doubleSided = twoSided != 0;

        if (index < textures.size() && textures[index])
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
            textures[index]->GetDesc(&viewDescription);
            material.decodeSrgb = !IsSrgbFormat(viewDescription.Format);
        }
    }

    laceValid_ = materials_.size() > 4 && IsLaceMaterial({ materials_[4].mode,
        materials_[4].cutoff, 4, materials_[4].name, false });
    if (laceValid_)
    {
        settings_.laceCutoff = materials_[4].cutoff;
        comparisonReason_.clear();
    }
    else
    {
        comparisonReason_ = "Lace comparison disabled: material 4 name or authored MASK mode does not match.";
    }
    return true;
}

bool App::BuildPosedCentroids()
{
    std::vector<VertexSkinnedTBN> vertices;
    std::vector<UINT> indices;
    std::vector<XMFLOAT4X4> palette;
    if (!ReadBuffer(device_.Get(), context_.Get(), character_->GetVertexBuffer(), vertices) ||
        !ReadBuffer(device_.Get(), context_.Get(), character_->GetIndexBuffer(), indices) ||
        !ReadBuffer(device_.Get(), context_.Get(), character_->GetBoneConstantBuffer(), palette))
        return false;

    std::vector<XMFLOAT3> posed(vertices.size());
    for (std::size_t index = 0; index < vertices.size(); ++index)
    {
        const VertexSkinnedTBN& vertex = vertices[index];
        const float weights[]{ vertex.boneWeight.x, vertex.boneWeight.y,
            vertex.boneWeight.z, vertex.boneWeight.w };
        XMVECTOR position = XMVectorZero();
        for (int influence = 0; influence < 4; ++influence)
        {
            if (vertex.boneIdx[influence] >= palette.size())
                return false;
            position += XMVector4Transform(
                XMVectorSet(vertex.pos.x, vertex.pos.y, vertex.pos.z, 1.0f),
                XMLoadFloat4x4(&palette[vertex.boneIdx[influence]])) * weights[influence];
        }
        XMStoreFloat3(&posed[index], position);
        if (!std::isfinite(posed[index].x) || !std::isfinite(posed[index].y) ||
            !std::isfinite(posed[index].z))
            return false;
    }

    centroids_.clear();
    for (const auto& subset : character_->GetSubsets())
    {
        if (subset.indexCount == 0 ||
            std::size_t(subset.startIndex) + subset.indexCount > indices.size())
            return false;
        XMVECTOR sum = XMVectorZero();
        for (UINT offset = 0; offset < subset.indexCount; ++offset)
        {
            const UINT vertexIndex = indices[subset.startIndex + offset];
            if (vertexIndex >= posed.size())
                return false;
            sum += XMLoadFloat3(&posed[vertexIndex]);
        }
        XMFLOAT3 centroid;
        XMStoreFloat3(&centroid, sum / static_cast<float>(subset.indexCount));
        centroids_.push_back(centroid);
    }
    return true;
}

bool App::Resize(UINT width, UINT height)
{
    targetsMatch_ = false;
    configurationError_.clear();
    configurationHr_ = S_OK;
    if (!swapChain_ || width == 0 || height == 0)
        return true;

    auto handleFailure = [&](ResizeFailureStage stage, const wchar_t* operation,
        const char* recoverableMessage, HRESULT operationResult) {
        const ResizeFailureDecision decision = ClassifyResizeFailure(stage,
            operationResult, device_->GetDeviceRemovedReason());
        configurationHr_ = decision.reportedResult;
        if (decision.fatal)
        {
            Fail(operation, decision.reportedResult);
            PostQuitMessage(1);
        }
        else
        {
            configurationError_ = recoverableMessage;
            OutputDebugStringA(configurationError_.c_str());
        }
        return false;
    };

    context_->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* nullViews[8]{};
    context_->PSSetShaderResources(0, 8, nullViews);
    context_->VSSetShaderResources(0, 8, nullViews);
    backbuffer_.Reset();

    HRESULT result = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(result))
        return handleFailure(ResizeFailureStage::ResizeBuffers,
            L"IDXGISwapChain::ResizeBuffers / D3D device loss",
            "IDXGISwapChain::ResizeBuffers failed", result);
    ComPtr<ID3D11Texture2D> texture;
    result = swapChain_->GetBuffer(0, IID_PPV_ARGS(&texture));
    if (FAILED(result))
        return handleFailure(ResizeFailureStage::GetBuffer,
            L"IDXGISwapChain::GetBuffer / D3D device loss",
            "IDXGISwapChain::GetBuffer failed after resize", result);
    result = device_->CreateRenderTargetView(texture.Get(), nullptr, &backbuffer_);
    if (FAILED(result))
        return handleFailure(ResizeFailureStage::BackbufferTarget,
            L"ID3D11Device::CreateRenderTargetView(backbuffer) / D3D device loss",
            "CreateRenderTargetView(backbuffer) failed", result);
    if (!pipeline_.Configure(device_.Get(), width, height, settings_.mode))
        return handleFailure(ResizeFailureStage::PipelineConfigure,
            L"MsaaPipeline::Configure / D3D device loss",
            "MsaaPipeline::Configure failed after resize; rendering is paused", E_FAIL);
    targetsMatch_ = pipeline_.Width() == width && pipeline_.Height() == height;
    return targetsMatch_;
}

void App::OnUpdate(const float& dt)
{
    if (orbit_ && !minimized_)
        orbitAngle_ += dt * 0.3f;
}

void App::BuildDraws()
{
    const bool diagnostic = scene_ == Scene::Diagnostic;
    constexpr float targetX = 0.0f;
    const float targetY = diagnostic ? 0.22f : (closeUp_ ? 0.68f : 0.80f);
    const float targetZ = diagnostic ? 0.14f : 0.0f;
    const float distance = diagnostic ? 2.45f : (closeUp_ ? 1.25f : 3.70f);
    const XMVECTOR target = XMVectorSet(targetX, targetY, targetZ, 1.0f);
    const XMVECTOR eye = target + XMVectorSet(std::sin(orbitAngle_) * distance,
        0.10f, -std::cos(orbitAngle_) * distance, 0.0f);
    const XMMATRIX view = XMMatrixLookAtLH(eye, target, XMVectorSet(0, 1, 0, 0));
    const float aspect = SceneWidth(m_ClientWidth) /
        std::max(1.0f, static_cast<float>(m_ClientHeight));
    const float fov = 2.0f * std::atan(std::tan(XMConvertToRadians(36.0f) * 0.5f) *
        std::max(1.0f, 1.15f / aspect));
    XMStoreFloat4x4(&view_, view);
    XMStoreFloat4x4(&projection_,
        XMMatrixPerspectiveFovLH(fov, aspect, NearPlane, FarPlane));

    opaqueDraws_.clear();
    draws_.clear();
    transparentDraws_.clear();
    auto add = [&](const Draw& draw, XMVECTOR worldCenter) {
        if (draw.material.rule == SurfaceRule::Blend)
        {
            const std::size_t id = draws_.size();
            draws_.push_back(draw);
            transparentDraws_.push_back({ id,
                XMVectorGetZ(XMVector3TransformCoord(worldCenter, view)) });
        }
        else
        {
            opaqueDraws_.push_back(draw);
        }
    };

    if (!diagnostic)
    {
        const auto& subsets = character_->GetSubsets();
        for (std::size_t index = 0; index < subsets.size(); ++index)
        {
            const std::size_t materialIndex = subsets[index].materialIndex;
            if (materialIndex >= materials_.size() || index >= centroids_.size())
                continue;
            const Material& material = materials_[materialIndex];
            const EffectiveMaterial effective = ResolveMaterial({ material.mode,
                material.cutoff, materialIndex, material.name, false }, settings_);
            add({ true, index, effective }, XMVector3TransformCoord(
                XMLoadFloat3(&centroids_[index]), XMLoadFloat4x4(&characterWorld_)));
        }
    }
    else
    {
        for (std::size_t index = 0; index < diagnosticSurfaces_.size(); ++index)
        {
            const DiagnosticSurface& surface = diagnosticSurfaces_[index];
            const EffectiveMaterial effective = ResolveMaterial({
                surface.pattern ? AlphaMode::Mask : AlphaMode::Opaque,
                settings_.diagnosticCutoff, index, "Diagnostic surface",
                surface.pattern }, settings_);
            add({ false, index, effective }, XMVector3TransformCoord(
                XMVectorZero(), XMLoadFloat4x4(&surface.world)));
        }
    }
    OrderTransparentDraws(transparentDraws_);
}

void App::DrawSurface(const Draw& draw)
{
    SurfaceConstants constants{};
    XMStoreFloat4x4(&constants.view, XMMatrixTranspose(XMLoadFloat4x4(&view_)));
    XMStoreFloat4x4(&constants.projection,
        XMMatrixTranspose(XMLoadFloat4x4(&projection_)));
    constants.material = { static_cast<float>(draw.material.rule),
        draw.material.cutoff, draw.material.opacity, draw.character ? 1.0f : 0.0f };

    ID3D11Buffer* vertexBuffer = diagnosticVertices_.Get();
    ID3D11Buffer* indexBuffer = diagnosticIndices_.Get();
    ID3D11ShaderResourceView* texture = nullptr;
    ID3D11RasterizerState* rasterizer = cullBack_.Get();
    UINT indexCount = 6;
    UINT firstIndex = 0;
    if (draw.character)
    {
        const auto& subset = character_->GetSubsets()[draw.index];
        const Material& material = materials_[subset.materialIndex];
        const auto& textures = character_->GetMaterialSRVs();
        texture = subset.materialIndex < textures.size() ? textures[subset.materialIndex] : nullptr;
        constants.surface.x = texture ? 1.0f : 0.0f;
        constants.surface.y = material.decodeSrgb ? 1.0f : 0.0f;
        constants.baseColor = material.factor;
        XMStoreFloat4x4(&constants.world,
            XMMatrixTranspose(XMLoadFloat4x4(&characterWorld_)));
        vertexBuffer = character_->GetVertexBuffer();
        indexBuffer = character_->GetIndexBuffer();
        rasterizer = material.doubleSided ? cullNone_.Get() : cullBack_.Get();
        indexCount = subset.indexCount;
        firstIndex = subset.startIndex;
    }
    else
    {
        const DiagnosticSurface& surface = diagnosticSurfaces_[draw.index];
        XMStoreFloat4x4(&constants.world,
            XMMatrixTranspose(XMLoadFloat4x4(&surface.world)));
        constants.baseColor = surface.color;
        if (surface.pattern)
        {
            texture = diagnosticPattern_.Get();
            constants.surface.x = 1.0f;
            // Procedural white RGB is authored in linear space; alpha is never decoded.
            constants.surface.y = 0.0f;
        }
        rasterizer = surface.doubleSided ? cullNone_.Get() : cullBack_.Get();
    }

    context_->UpdateSubresource(constants_.Get(), 0, nullptr, &constants, 0, 0);
    const UINT stride = sizeof(VertexSkinnedTBN);
    const UINT offset = 0;
    context_->IASetInputLayout(layout_.Get());
    context_->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context_->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(surfaceVs_.Get(), nullptr, 0);
    context_->PSSetShader(surfacePs_.Get(), nullptr, 0);
    context_->GSSetShader(nullptr, nullptr, 0);
    context_->HSSetShader(nullptr, nullptr, 0);
    context_->DSSetShader(nullptr, nullptr, 0);
    ID3D11Buffer* vertexConstants[]{ constants_.Get(), character_->GetBoneConstantBuffer() };
    ID3D11Buffer* pixelConstants = constants_.Get();
    context_->VSSetConstantBuffers(0, 2, vertexConstants);
    context_->PSSetConstantBuffers(0, 1, &pixelConstants);
    context_->PSSetShaderResources(0, 1, &texture);
    ID3D11SamplerState* sampler = sampler_.Get();
    context_->PSSetSamplers(0, 1, &sampler);
    context_->RSSetState(rasterizer);
    const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, SceneWidth(m_ClientWidth),
        static_cast<float>(m_ClientHeight), 0.0f, 1.0f };
    context_->RSSetViewports(1, &viewport);
    context_->DrawIndexed(indexCount, firstIndex, 0);
}

void App::OnRender()
{
    if (failed_ || minimized_ || !targetsMatch_ || !backbuffer_ || !imgui_)
        return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    RenderHud();
    BuildDraws();

    pipeline_.BeginFrame(context_.Get(), background_);
    for (const Draw& draw : opaqueDraws_)
    {
        pipeline_.BeginSurface(context_.Get(), draw.material.rule);
        DrawSurface(draw);
    }
    for (const TransparentDraw& sorted : transparentDraws_)
    {
        const Draw& draw = draws_[sorted.id];
        pipeline_.BeginSurface(context_.Get(), SurfaceRule::Blend);
        DrawSurface(draw);
    }
    pipeline_.EndScene(context_.Get());
    pipeline_.Resolve(context_.Get());
    pipeline_.Present(context_.Get(), backbuffer_.Get(), exposure_);

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    const HRESULT result = swapChain_->Present(1, 0);
    if (FAILED(result))
    {
        HRESULT reported = result;
        const wchar_t* operation = L"IDXGISwapChain::Present";
        if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
        {
            operation = L"IDXGISwapChain::Present / D3D device removed";
            const HRESULT removalReason = device_->GetDeviceRemovedReason();
            if (FAILED(removalReason))
                reported = removalReason;
        }
        Fail(operation, reported);
        PostQuitMessage(1);
    }
}

void App::RenderHud()
{
    const float sceneWidth = SceneWidth(m_ClientWidth);
    const float panelWidth = static_cast<float>(m_ClientWidth) - sceneWidth;
    ImGui::SetNextWindowPos(ImVec2(sceneWidth + 8.0f, 12.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(std::max(1.0f, panelWidth - 16.0f),
        std::max(1.0f, m_ClientHeight - 24.0f)), ImGuiCond_Always);
    ImGui::Begin("40 / MSAA Alpha-to-Coverage", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::TextWrapped("Same fixed pose, shader, lighting, camera, and alpha inputs across modes.");

    int requestedMode = static_cast<int>(settings_.mode);
    if (ImGui::Combo("Mode [1/2/3]", &requestedMode,
            "Alpha Test 1x\0Alpha Test 4x MSAA\0Alpha-to-Coverage 4x MSAA\0"))
    {
        const Mode candidate = static_cast<Mode>(requestedMode);
        if (SampleCount(candidate) == 4 && !pipeline_.Support().supports4x)
        {
            configurationError_ = "4x selection blocked: " + Narrow(pipeline_.Support().reason);
        }
        else if (pipeline_.Configure(device_.Get(), m_ClientWidth, m_ClientHeight, candidate))
        {
            settings_.mode = candidate;
            configurationError_.clear();
            targetsMatch_ = true;
        }
        else
        {
            configurationError_ = "Mode change failed; the previous mode remains active.";
        }
    }

    int requestedScene = static_cast<int>(scene_);
    if (ImGui::Combo("Scene [S]", &requestedScene, "Character\0Diagnostic\0"))
        scene_ = static_cast<Scene>(requestedScene);
    bool requestedCloseUp = closeUp_;
    if (ImGui::Checkbox("Lace close-up [C]", &requestedCloseUp))
    {
        closeUp_ = requestedCloseUp;
        scene_ = Scene::Character;
    }
    ImGui::Checkbox("Orbit camera [O]", &orbit_);

    ImGui::TextUnformatted("Comparison alpha multiplier");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##AlphaMultiplier", &settings_.alphaMultiplier, 0.0f, 1.0f);
    ImGui::TextWrapped("Applied only to guarded lace and the diagnostic alpha pattern.");

    const bool alphaToCoverage = settings_.mode == Mode::AlphaToCoverage4x;
    ImGui::BeginDisabled(alphaToCoverage);
    float* cutoff = scene_ == Scene::Character ? &settings_.laceCutoff :
        &settings_.diagnosticCutoff;
    ImGui::TextUnformatted(scene_ == Scene::Character ? "Lace cutoff" : "Diagnostic cutoff");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##Cutoff", cutoff, 0.0f, 1.0f);
    ImGui::EndDisabled();
    if (alphaToCoverage)
        ImGui::TextWrapped("Cutoff is disabled in A2C; its Alpha Test value is retained.");

    ImGui::TextUnformatted("Background (linear)");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::ColorEdit3("##Background", background_);
    ImGui::TextUnformatted("Exposure");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##Exposure", &exposure_, 0.1f, 3.0f);

    ImGui::Separator();
    ImGui::Text("Fixed pose: %s / 0.5 s", poseName_.c_str());
    ImGui::Text("Actual sample count: %u", pipeline_.Samples());
    if (pipeline_.Support().supports4x)
        ImGui::TextUnformatted("4x RGBA16F + D32: supported");
    else
        ImGui::TextWrapped("4x unavailable: %s", Narrow(pipeline_.Support().reason).c_str());
    const double mebibytes = static_cast<double>(pipeline_.MemoryBytes()) /
        (1024.0 * 1024.0);
    ImGui::Text("Logical target estimate: %.2f MiB", mebibytes);
    ImGui::TextWrapped("Color/depth/resolve estimate only; not total VRAM usage.");

    const GpuTimings& timing = pipeline_.Timings();
    if (!timing.available)
    {
        ImGui::TextUnformatted("GPU timings: unavailable (rendering continues)");
    }
    else if (!timing.valid || timing.mode != settings_.mode ||
        timing.width != pipeline_.Width() || timing.height != pipeline_.Height())
    {
        ImGui::TextUnformatted("GPU timings: loading / no completed current sample");
        ImGui::TextUnformatted(pipeline_.Samples() == 1 ? "Resolve: N/A (1x)" :
            "Resolve: loading");
    }
    else
    {
        ImGui::Text("Scene GPU (last completed): %.3f ms", timing.sceneMs);
        if (timing.resolveApplicable)
            ImGui::Text("Resolve GPU (last completed): %.3f ms", timing.resolveMs);
        else
            ImGui::TextUnformatted("Resolve: N/A (1x)");
    }

    if (!laceValid_)
        ImGui::TextWrapped("%s Character keeps its authored material; Diagnostic remains active.",
            comparisonReason_.c_str());
    else
        ImGui::TextWrapped("Lace guard verified: material 4, full name, authored MASK; cutoff %.3f.",
            settings_.laceCutoff);
    if (!configurationError_.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.25f, 1.0f), "%s",
            configurationError_.c_str());
    ImGui::TextWrapped("Diagnostic: diagonal opaque edge, alpha ramp/stripes, foreground occluder.");
    if (capture_)
        ImGui::TextUnformatted("README capture / stationary until O");
    ImGui::End();
}

void App::HandleKey(WPARAM key)
{
    if (key >= '1' && key <= '3')
    {
        const Mode candidate = static_cast<Mode>(key - '1');
        if (SampleCount(candidate) == 4 && !pipeline_.Support().supports4x)
        {
            configurationError_ = "4x shortcut blocked: " + Narrow(pipeline_.Support().reason);
        }
        else if (pipeline_.Configure(device_.Get(), m_ClientWidth, m_ClientHeight, candidate))
        {
            settings_.mode = candidate;
            configurationError_.clear();
            targetsMatch_ = true;
        }
        else
        {
            configurationError_ = "Mode shortcut failed; the previous mode remains active.";
        }
    }
    if (key == 'O')
        orbit_ = !orbit_;
    if (key == 'C')
    {
        closeUp_ = !closeUp_;
        scene_ = Scene::Character;
    }
    if (key == 'S')
        scene_ = scene_ == Scene::Character ? Scene::Diagnostic : Scene::Character;
}

void App::OnInputProcess(const Keyboard::State&,
    const Keyboard::KeyboardStateTracker&, const Mouse::State&,
    const Mouse::ButtonStateTracker&)
{
    // WM_KEYDOWN owns discrete shortcuts so a complete down/up pair cannot be drained.
}

LRESULT CALLBACK App::WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_SIZE)
    {
        m_ClientWidth = LOWORD(lParam);
        m_ClientHeight = HIWORD(lParam);
        minimized_ = wParam == SIZE_MINIMIZED || m_ClientWidth == 0 || m_ClientHeight == 0;
        if (minimized_)
        {
            targetsMatch_ = false;
        }
        else if (swapChain_ && !failed_)
        {
            Resize(m_ClientWidth, m_ClientHeight);
        }
    }
    if (message == WM_KEYDOWN && !(lParam & (1LL << 30)) &&
        (!ImGui::GetCurrentContext() || !ImGui::GetIO().WantCaptureKeyboard))
        HandleKey(wParam);
    return GameApp::WndProc(window, message, wParam, lParam);
}

void App::OnUninitialize()
{
    if (imgui_)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        imgui_ = false;
    }
    if (context_)
    {
        context_->ClearState();
        context_->Flush();
    }
    character_.reset();
    pipeline_ = MsaaPipeline{};
    backbuffer_.Reset();
    surfaceVs_.Reset();
    surfacePs_.Reset();
    layout_.Reset();
    constants_.Reset();
    diagnosticVertices_.Reset();
    diagnosticIndices_.Reset();
    diagnosticPattern_.Reset();
    cullBack_.Reset();
    cullNone_.Reset();
    sampler_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
}
}
