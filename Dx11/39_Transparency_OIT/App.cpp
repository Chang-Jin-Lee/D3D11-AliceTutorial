#include "App.h"
#include "../Common/Mesh/FbxModel.h"
#include "../Common/Animation/Animator.h"
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
#include <chrono>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cwchar>

using namespace DirectX;
using namespace Transparency39;
using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float NearPlane = 0.05f;
    constexpr float FarPlane = 12.0f;
    static_assert(sizeof(VertexSkinnedTBN) == 96);
    static_assert(offsetof(VertexSkinnedTBN, boneIdx) == 72);
    static_assert(offsetof(VertexSkinnedTBN, boneWeight) == 80);

    bool Compile(const char* entry, const char* target, ComPtr<ID3DBlob>& bytecode)
    {
        ComPtr<ID3DBlob> errors;
        const HRESULT hr = D3DCompileFromFile(L"39_Surface.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entry, target, D3DCOMPILE_ENABLE_STRICTNESS, 0, bytecode.ReleaseAndGetAddressOf(), errors.GetAddressOf());
        if (FAILED(hr) && errors)
        {
            OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
            MessageBoxA(GameApp::m_hWnd, static_cast<const char*>(errors->GetBufferPointer()),
                        "39 Surface shader compilation", MB_OK | MB_ICONERROR);
        }
        return SUCCEEDED(hr);
    }

    template<typename T>
    bool ReadBuffer(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Buffer* source,
                    std::vector<T>& result)
    {
        if (!source) return false;
        D3D11_BUFFER_DESC desc{};
        source->GetDesc(&desc);
        const UINT byteWidth = desc.ByteWidth;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        ComPtr<ID3D11Buffer> staging;
        if (FAILED(device->CreateBuffer(&desc, nullptr, staging.GetAddressOf()))) return false;
        context->CopyResource(staging.Get(), source);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return false;
        result.resize(byteWidth / sizeof(T));
        std::memcpy(result.data(), mapped.pData, result.size() * sizeof(T));
        context->Unmap(staging.Get(), 0);
        return true;
    }
}

App::App()
{
    wcscpy_s(m_szTitle, L"39. Transparency OIT - Alice Tutorial");
    wcscpy_s(m_szWindowClass, L"AliceTransparency39");
    XMStoreFloat4x4(&characterWorld_, XMMatrixRotationY(-0.15f));
}

App::~App() { OnUninitialize(); }

bool App::Fail(const wchar_t* operation, HRESULT hr)
{
    if (!failed_)
    {
        wchar_t message[512]{};
        swprintf_s(message, L"%s\nHRESULT: 0x%08X", operation, static_cast<unsigned>(hr));
        OutputDebugStringW(message);
        MessageBoxW(m_hWnd, message, L"39 Transparency OIT", MB_OK | MB_ICONERROR);
    }
    failed_ = true;
    return false;
}

bool App::OnInitialize()
{
    capture_ = ReadmeCapture::IsEnabled();
    // Both interactive and capture sessions start stationary; only explicit O/UI input starts orbit.
    orbit_ = false;
    if (!CreateDevice()) return false;
    if (!pipeline_.Initialize(device_.Get(), L".")) return Fail(L"Could not initialize OIT pipeline / 39_Resolve.hlsl.");
    if (!CreateSurfaceResources()) return Fail(L"Could not create surface shaders or geometry.");
    if (!LoadCharacter()) return Fail(L"Could not load SampleModel.glb, its materials, or the fixed-pose buffers.");
    if (!Resize(m_ClientWidth, m_ClientHeight)) return false;
    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext()) return Fail(L"Could not create ImGui context.");
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplWin32_Init(m_hWnd))
    {
        ImGui::DestroyContext();
        return Fail(L"Could not initialize ImGui Win32.");
    }
    if (!ImGui_ImplDX11_Init(device_.Get(), context_.Get()))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return Fail(L"Could not initialize ImGui D3D11.");
    }
    imgui_ = true;
    return true;
}

bool App::CreateDevice()
{
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Width = m_ClientWidth;
    desc.BufferDesc.Height = m_ClientHeight;
    // Present shader encodes sRGB; the output RTV must remain UNORM.
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = m_hWnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    UINT flags = 0;
#if defined(_DEBUG)
    flags = D3D11_CREATE_DEVICE_DEBUG;
#endif
    const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    auto create = [&]() {
        return D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            &level, 1, D3D11_SDK_VERSION, &desc, swapChain_.ReleaseAndGetAddressOf(),
            device_.ReleaseAndGetAddressOf(), nullptr, context_.ReleaseAndGetAddressOf());
    };
    HRESULT hr = create();
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING) { flags = 0; hr = create(); }
    return SUCCEEDED(hr) || Fail(L"D3D11 device / swap chain creation failed.", hr);
}

bool App::CreateSurfaceResources()
{
    ComPtr<ID3DBlob> blob;
    if (!Compile("VSMain", "vs_5_0", blob) || FAILED(device_->CreateVertexShader(
        blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, surfaceVs_.GetAddressOf()))) return false;
    const D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VertexSkinnedTBN,pos),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VertexSkinnedTBN,n),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(VertexSkinnedTBN,color),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(VertexSkinnedTBN,uv),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BLENDINDICES",0,DXGI_FORMAT_R16G16B16A16_UINT,0,offsetof(VertexSkinnedTBN,boneIdx),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BLENDWEIGHT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(VertexSkinnedTBN,boneWeight),D3D11_INPUT_PER_VERTEX_DATA,0}
    };
    if (FAILED(device_->CreateInputLayout(elements, static_cast<UINT>(std::size(elements)),
        blob->GetBufferPointer(), blob->GetBufferSize(), layout_.GetAddressOf()))) return false;
    if (!Compile("PSMain", "ps_5_0", blob) || FAILED(device_->CreatePixelShader(
        blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, surfacePs_.GetAddressOf()))) return false;
    if (!Compile("PSOit", "ps_5_0", blob) || FAILED(device_->CreatePixelShader(
        blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, oitPs_.GetAddressOf()))) return false;

    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = sizeof(SurfaceConstants);
    buffer.Usage = D3D11_USAGE_DEFAULT;
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(device_->CreateBuffer(&buffer, nullptr, constants_.GetAddressOf()))) return false;
    std::array<VertexSkinnedTBN,4> vertices{};
    vertices[0].pos = {-0.5f,-0.5f,0}; vertices[0].uv = {0,1};
    vertices[1].pos = {-0.5f, 0.5f,0}; vertices[1].uv = {0,0};
    vertices[2].pos = { 0.5f, 0.5f,0}; vertices[2].uv = {1,0};
    vertices[3].pos = { 0.5f,-0.5f,0}; vertices[3].uv = {1,1};
    for (auto& v : vertices) { v.n = {0,0,-1}; v.color = {1,1,1,1}; }
    const UINT indices[]{0,1,2,0,2,3};
    buffer.Usage = D3D11_USAGE_IMMUTABLE;
    buffer.ByteWidth = sizeof(vertices); buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initial{ vertices.data(),0,0 };
    if (FAILED(device_->CreateBuffer(&buffer, &initial, planeVertices_.GetAddressOf()))) return false;
    buffer.ByteWidth = sizeof(indices); buffer.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initial.pSysMem = indices;
    if (FAILED(device_->CreateBuffer(&buffer, &initial, planeIndices_.GetAddressOf()))) return false;
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_BACK;
    raster.DepthClipEnable = TRUE;
    if (FAILED(device_->CreateRasterizerState(&raster, cullBack_.GetAddressOf()))) return false;
    raster.CullMode = D3D11_CULL_NONE;
    if (FAILED(device_->CreateRasterizerState(&raster, cullNone_.GetAddressOf()))) return false;
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device_->CreateSamplerState(&sampler, sampler_.GetAddressOf()))) return false;

    auto addPlane = [&](float width, float height, float yaw, float roll, XMFLOAT3 center,
                        XMFLOAT4 color, bool transparent) {
        Plane plane{};
        XMStoreFloat4x4(&plane.world, XMMatrixScaling(width,height,1) * XMMatrixRotationY(yaw) *
            XMMatrixRotationZ(roll) * XMMatrixTranslation(center.x,center.y,center.z));
        plane.color = color;
        plane.transparent = transparent;
        planes_.push_back(plane);
    };
    // All three tinted sheets cross near one center; no single object order can solve them.
    addPlane(1.0f,1.0f, 0.70f, 0.10f, {1.25f,0.83f,0.03f}, {1.0f,0.08f,0.05f,0.42f}, true);
    addPlane(1.0f,1.0f,-0.65f,-0.15f, {1.25f,0.83f,0.00f}, {0.04f,0.8f,0.18f,0.42f}, true);
    addPlane(1.0f,1.0f, 0.05f, 0.50f, {1.25f,0.83f,0.06f}, {0.06f,0.23f,1.0f,0.42f}, true);
    addPlane(0.25f,0.50f,0,0,{1.04f,0.67f,-0.65f},{0.72f,0.65f,0.40f,1},false);
    addPlane(1.52f,1.45f,0,0,{1.25f,0.80f,0.80f},{0.22f,0.26f,0.32f,1},false);
    return true;
}

bool App::LoadCharacter()
{
    character_ = std::make_unique<FbxModel>();
    if (!character_->Load(device_.Get(), L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb") ||
        !character_->HasMesh() || character_->GetVertexStride() != sizeof(VertexSkinnedTBN) ||
        !character_->GetBoneConstantBuffer()) return false;
    const auto& names = character_->GetAnimationNames();
    int selected = 0, fallback = -1;
    bool foundIdle = false;
    for (std::size_t i=0; i<names.size(); ++i)
    {
        std::string name = names[i];
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c) { return char(std::tolower(c)); });
        if (name.find("idle") != std::string::npos) { selected = static_cast<int>(i); foundIdle = true; break; }
        if (name == "vrm_1") fallback = static_cast<int>(i);
    }
    if (!foundIdle && fallback >= 0) selected = fallback;
    if (!names.empty()) character_->SetCurrentAnimation(selected);
    poseName_ = names.empty() ? "Bind pose" : names[selected];
    character_->SetAnimationPlaying(false);
    character_->SetAnimationTimeSeconds(0.5);
    const aiScene* scene = character_->GetScenePtr();
    if (!scene) return false;
    // FbxModel's legacy animation fallback lacks its shared channel context. Evaluate the
    // selected clip through the existing animator, as lesson 38 does, and retain only its palette.
    CharacterAnimator pose;
    pose.Initialize(device_.Get(),scene,character_->GetNodeIndexOfName(),character_->GetGlobalInverse(),
                    character_->GetBoneNames(),character_->GetBoneOffsets());
    if (!pose.GetBoneCB()) return false;
    const aiAnimation* clip = !names.empty() && static_cast<unsigned>(selected) < scene->mNumAnimations ?
        scene->mAnimations[selected] : nullptr;
    pose.UpdateAnimation(0.0f,clip,0.5f,clip,0.5f,0.0f);
    pose.UploadPalette(context_.Get(),pose.finalTransforms);
    context_->CopyResource(character_->GetBoneConstantBuffer(),pose.GetBoneCB());
    if (!BuildPosedCentroids()) return false;

    const auto& alpha = character_->GetMaterialAlphaInfos();
    const auto& textures = character_->GetMaterialSRVs();
    if (alpha.size() != scene->mNumMaterials) return false;
    materials_.resize(scene->mNumMaterials);
    for (std::size_t i=0; i<materials_.size(); ++i)
    {
        Material& material = materials_[i];
        aiString name;
        scene->mMaterials[i]->Get(AI_MATKEY_NAME, name);
        material.name = name.C_Str();
        material.mode = static_cast<AlphaMode>(alpha[i].mode);
        material.cutoff = alpha[i].cutoff;
        aiColor4D factor(1,1,1,1);
        // Shared FbxMaterial loads texture bytes without baking this linear factor.
        scene->mMaterials[i]->Get(AI_MATKEY_BASE_COLOR, factor);
        material.factor = {factor.r,factor.g,factor.b,factor.a};
        int twoSided = 0;
        scene->mMaterials[i]->Get(AI_MATKEY_TWOSIDED, twoSided);
        material.doubleSided = twoSided != 0;
        if (i < textures.size() && textures[i])
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
            textures[i]->GetDesc(&desc);
            material.decodeSrgb = desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
                desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB && desc.Format != DXGI_FORMAT_BC1_UNORM_SRGB &&
                desc.Format != DXGI_FORMAT_BC2_UNORM_SRGB && desc.Format != DXGI_FORMAT_BC3_UNORM_SRGB &&
                desc.Format != DXGI_FORMAT_BC7_UNORM_SRGB;
        }
    }
    laceValid_ = materials_.size() > 4 && IsLaceMaterial(4,materials_[4].name) && materials_[4].mode == AlphaMode::Mask;
    return true;
}

bool App::BuildPosedCentroids()
{
    std::vector<VertexSkinnedTBN> vertices;
    std::vector<UINT> indices;
    std::vector<XMFLOAT4X4> palette;
    if (!ReadBuffer(device_.Get(),context_.Get(),character_->GetVertexBuffer(),vertices) ||
        !ReadBuffer(device_.Get(),context_.Get(),character_->GetIndexBuffer(),indices) ||
        !ReadBuffer(device_.Get(),context_.Get(),character_->GetBoneConstantBuffer(),palette)) return false;
    std::vector<XMFLOAT3> posed(vertices.size());
    for (std::size_t i=0; i<vertices.size(); ++i)
    {
        const auto& v = vertices[i];
        const float weights[]{v.boneWeight.x,v.boneWeight.y,v.boneWeight.z,v.boneWeight.w};
        XMVECTOR position = XMVectorZero();
        for (int j=0; j<4; ++j)
        {
            if (v.boneIdx[j] >= palette.size()) return false;
            // Exact uploaded row-major bytes, same four weighted transforms as VSMain.
            position += XMVector4Transform(XMVectorSet(v.pos.x,v.pos.y,v.pos.z,1),
                XMLoadFloat4x4(&palette[v.boneIdx[j]])) * weights[j];
        }
        XMStoreFloat3(&posed[i],position);
        if (!std::isfinite(posed[i].x) || !std::isfinite(posed[i].y) || !std::isfinite(posed[i].z)) return false;
    }
    for (const auto& subset : character_->GetSubsets())
    {
        if (subset.indexCount == 0 || std::size_t(subset.startIndex)+subset.indexCount > indices.size()) return false;
        XMVECTOR sum = XMVectorZero();
        for (UINT j=0; j<subset.indexCount; ++j)
        {
            const UINT vertex = indices[subset.startIndex+j];
            if (vertex >= posed.size()) return false;
            sum += XMLoadFloat3(&posed[vertex]);
        }
        XMFLOAT3 center;
        XMStoreFloat3(&center,sum / static_cast<float>(subset.indexCount));
        centroids_.push_back(center);
    }
    return true;
}

bool App::Resize(UINT width, UINT height)
{
    if (!swapChain_ || width == 0 || height == 0) return true;
    context_->OMSetRenderTargets(0,nullptr,nullptr);
    ID3D11ShaderResourceView* nullViews[8]{};
    context_->PSSetShaderResources(0,8,nullViews);
    context_->VSSetShaderResources(0,8,nullViews);
    backbuffer_.Reset();
    HRESULT hr = swapChain_->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0);
    if (FAILED(hr)) return Fail(L"Swap chain resize failed.",hr);
    ComPtr<ID3D11Texture2D> texture;
    hr = swapChain_->GetBuffer(0,IID_PPV_ARGS(texture.GetAddressOf()));
    if (FAILED(hr)) return Fail(L"Could not access resized backbuffer.",hr);
    hr = device_->CreateRenderTargetView(texture.Get(),nullptr,backbuffer_.GetAddressOf());
    if (FAILED(hr)) return Fail(L"Could not create UNORM backbuffer RTV.",hr);
    if (!pipeline_.Resize(device_.Get(),width,height)) return Fail(L"Could not resize OIT render targets.");
    return true;
}

float App::SceneWidth() const
{
    return std::max(1.0f, static_cast<float>(m_ClientWidth) - std::min(370.0f, m_ClientWidth * 0.36f));
}

void App::OnUpdate(const float& dt)
{
    if (orbit_ && !minimized_) orbitAngle_ += dt * 0.3f;
}

void App::BuildDraws()
{
    const float targetX = closeUp_ ? 0.0f : (scene_ == 2 ? 1.25f : (scene_ == 1 ? 0.0f : 0.60f));
    const float targetY = closeUp_ ? 0.68f : 0.80f;
    const float distance = closeUp_ ? 1.25f : 3.70f;
    const XMVECTOR target = XMVectorSet(targetX,targetY,0,1);
    const XMVECTOR eye = target + XMVectorSet(std::sin(orbitAngle_)*distance,0.10f,-std::cos(orbitAngle_)*distance,0);
    const XMMATRIX view = XMMatrixLookAtLH(eye,target,XMVectorSet(0,1,0,0));
    // Widen vertical field at narrow sizes to keep the complete lesson in the scene viewport.
    const float aspect = SceneWidth() / std::max(1.0f,static_cast<float>(m_ClientHeight));
    const float fov = 2.0f * std::atan(std::tan(XMConvertToRadians(36.0f)*0.5f) * std::max(1.0f,1.15f/aspect));
    XMStoreFloat4x4(&view_,view);
    XMStoreFloat4x4(&projection_,XMMatrixPerspectiveFovLH(fov,aspect,NearPlane,FarPlane));
    settings_.alphaTest = mode_ == Mode::AlphaTest;
    if (!laceValid_) settings_.laceExperiment = false;
    draws_.clear(); opaque_.clear(); transparent_.clear();
    auto add = [&](Draw draw, XMVECTOR worldCenter) {
        const std::size_t id = draws_.size();
        draws_.push_back(draw);
        if (draw.material.mode == AlphaMode::Blend)
            transparent_.push_back({id,XMVectorGetZ(XMVector3TransformCoord(worldCenter,view))});
        else opaque_.push_back(id);
    };
    if (scene_ != 2)
    {
        const auto& subsets = character_->GetSubsets();
        for (std::size_t i=0; i<subsets.size(); ++i)
        {
            const auto materialIndex = subsets[i].materialIndex;
            if (materialIndex >= materials_.size()) continue;
            const auto& material = materials_[materialIndex];
            const auto effective = ResolveMaterial({material.mode,material.cutoff,materialIndex,material.name},settings_);
            add({true,i,effective},XMVector3TransformCoord(XMLoadFloat3(&centroids_[i]),XMLoadFloat4x4(&characterWorld_)));
        }
    }
    if (scene_ != 1 && !closeUp_)
    {
        for (std::size_t i=0; i<planes_.size(); ++i)
        {
            const auto effective = ResolveMaterial({planes_[i].transparent ? AlphaMode::Blend : AlphaMode::Opaque,
                0.5f,i,"Diagnostic plane",planes_[i].transparent},settings_);
            add({false,i,effective},XMVector3TransformCoord(XMVectorZero(),XMLoadFloat4x4(&planes_[i].world)));
        }
    }
    sortMs_ = 0;
    if (mode_ == Mode::SortedBlend)
    {
        const auto start = std::chrono::steady_clock::now();
        OrderTransparentDraws(transparent_,true,false);
        sortMs_ = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    }
    // Deliberately reverse the actual sorted result for the failure experiment. Not counted as sorting.
    OrderTransparentDraws(transparent_,false,reverse_);
}

void App::DrawSurface(const Draw& draw, bool oit)
{
    SurfaceConstants cb{};
    XMStoreFloat4x4(&cb.view,XMMatrixTranspose(XMLoadFloat4x4(&view_)));
    XMStoreFloat4x4(&cb.projection,XMMatrixTranspose(XMLoadFloat4x4(&projection_)));
    cb.material = {static_cast<float>(draw.material.mode),draw.material.cutoff,draw.material.opacity,draw.character ? 1.0f : 0.0f};
    cb.surface = {NearPlane,FarPlane,0,0};
    ID3D11Buffer* vertex = planeVertices_.Get();
    ID3D11Buffer* index = planeIndices_.Get();
    ID3D11ShaderResourceView* texture = nullptr;
    ID3D11RasterizerState* raster = cullNone_.Get();
    UINT count = 6, first = 0;
    if (draw.character)
    {
        const auto& subset = character_->GetSubsets()[draw.index];
        const auto& material = materials_[subset.materialIndex];
        const auto& textures = character_->GetMaterialSRVs();
        texture = subset.materialIndex < textures.size() ? textures[subset.materialIndex] : nullptr;
        cb.surface.z = texture ? 1.0f : 0.0f;
        cb.surface.w = material.decodeSrgb ? 1.0f : 0.0f;
        cb.baseColor = material.factor;
        XMStoreFloat4x4(&cb.world,XMMatrixTranspose(XMLoadFloat4x4(&characterWorld_)));
        vertex = character_->GetVertexBuffer(); index = character_->GetIndexBuffer();
        raster = material.doubleSided ? cullNone_.Get() : cullBack_.Get();
        count = subset.indexCount; first = subset.startIndex;
    }
    else
    {
        XMStoreFloat4x4(&cb.world,XMMatrixTranspose(XMLoadFloat4x4(&planes_[draw.index].world)));
        cb.baseColor = planes_[draw.index].color;
    }
    context_->UpdateSubresource(constants_.Get(),0,nullptr,&cb,0,0);
    const UINT stride = sizeof(VertexSkinnedTBN), offset = 0;
    context_->IASetInputLayout(layout_.Get());
    context_->IASetVertexBuffers(0,1,&vertex,&stride,&offset);
    context_->IASetIndexBuffer(index,DXGI_FORMAT_R32_UINT,0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(surfaceVs_.Get(),nullptr,0);
    context_->PSSetShader(oit ? oitPs_.Get() : surfacePs_.Get(),nullptr,0);
    context_->GSSetShader(nullptr,nullptr,0);
    context_->HSSetShader(nullptr,nullptr,0);
    context_->DSSetShader(nullptr,nullptr,0);
    ID3D11Buffer* buffers[]{constants_.Get(),character_->GetBoneConstantBuffer()};
    context_->VSSetConstantBuffers(0,2,buffers);
    context_->PSSetConstantBuffers(0,1,buffers);
    context_->PSSetShaderResources(0,1,&texture);
    ID3D11SamplerState* sampler = sampler_.Get();
    context_->PSSetSamplers(0,1,&sampler);
    context_->RSSetState(raster);
    const D3D11_VIEWPORT viewport{0,0,SceneWidth(),static_cast<float>(m_ClientHeight),0,1};
    context_->RSSetViewports(1,&viewport);
    context_->DrawIndexed(count,first,0);
}

void App::OnRender()
{
    if (failed_ || minimized_ || !backbuffer_ || !imgui_) return;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    RenderHud();
    BuildDraws();
    pipeline_.BeginOpaque(context_.Get(),background_);
    for (std::size_t id : opaque_) DrawSurface(draws_[id],false);
    pipeline_.BeginTransparency(context_.Get(),mode_);
    for (const auto& draw : transparent_) DrawSurface(draws_[draw.id],mode_ == Mode::WeightedOit);
    pipeline_.EndTransparency(context_.Get());
    ID3D11ShaderResourceView* nullView = nullptr;
    context_->PSSetShaderResources(0,1,&nullView);
    pipeline_.Resolve(context_.Get());
    pipeline_.Present(context_.Get(),backbuffer_.Get(),exposure_,debugView_);
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    const HRESULT hr = swapChain_->Present(1,0);
    if (FAILED(hr))
    {
        Fail(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ?
            L"Present failed: D3D device lost." : L"Swap chain Present failed.",hr);
        PostQuitMessage(1);
    }
}

void App::RenderHud()
{
    const float panelWidth = static_cast<float>(m_ClientWidth)-SceneWidth();
    ImGui::SetNextWindowPos(ImVec2(SceneWidth()+8,12),ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(std::max(1.0f,panelWidth-16),std::max(1.0f,m_ClientHeight-24.0f)),ImGuiCond_Always);
    ImGui::Begin("39 / Transparency OIT",nullptr,ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::TextWrapped("Fixed pose / same geometry and lighting");
    int mode = static_cast<int>(mode_);
    if (ImGui::Combo("Mode [1/2/3]",&mode,"Alpha Test\0Sorted Alpha Blend\0Weighted OIT\0"))
    {
        mode_ = static_cast<Mode>(mode);
        if (mode_ != Mode::WeightedOit) debugView_ = DebugView::Composite;
    }
    if (ImGui::Combo("Scene [S]",&scene_,"Both\0Character\0Planes\0") && scene_ == 2) closeUp_ = false;
    if (ImGui::Checkbox("Lace close-up [C]",&closeUp_) && closeUp_) scene_ = 1;
    ImGui::Checkbox("Orbit camera [O]",&orbit_);
    ImGui::BeginDisabled(!laceValid_);
    ImGui::Checkbox("Lace MASK -> BLEND [L]",&settings_.laceExperiment);
    ImGui::EndDisabled();
    if (!laceValid_) ImGui::TextWrapped("Lace experiment disabled: material 4 name or authored MASK mode does not match.");
    else ImGui::TextWrapped("Lace guard: material 4 + full name verified.");
    ImGui::Checkbox("Reverse draw order [R]",&reverse_);
    if (reverse_ && mode_ == Mode::SortedBlend)
        ImGui::TextColored(ImVec4(1,0.7f,0.3f,1),"Intentional WRONG sorted order");
    ImGui::TextUnformatted("Alpha multiplier");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##AlphaMultiplier",&settings_.alphaMultiplier,0,1);
    ImGui::TextWrapped("Alpha: lace experiment + planes. Eyes unchanged.");
    ImGui::TextUnformatted("Experiment cutoff");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##ExperimentCutoff",&settings_.experimentCutoff,0,1);
    ImGui::TextWrapped("Original MASK keeps its authored cutoff.");
    ImGui::TextUnformatted("Background (linear)");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::ColorEdit3("##Background",background_);
    ImGui::TextUnformatted("Exposure");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##Exposure",&exposure_,0.1f,3.0f);
    ImGui::BeginDisabled(mode_ != Mode::WeightedOit);
    int debug = static_cast<int>(debugView_);
    if (ImGui::Combo("OIT view",&debug,"Composite\0Accumulation\0Revealage\0")) debugView_ = static_cast<DebugView>(debug);
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::Text("Fixed pose: %s / 0.5 s",poseName_.c_str());
    ImGui::Text("Transparent draws: %zu",transparent_.size());
    if (mode_ == Mode::SortedBlend) ImGui::Text("CPU sort only: %.4f ms",sortMs_);
    else ImGui::TextUnformatted("CPU sort: 0 ms / not needed");
    const auto& timing = pipeline_.Timings();
    if (!timing.available) ImGui::TextUnformatted("GPU timing: unavailable");
    else if (!timing.valid) ImGui::TextUnformatted("GPU timing: warming up");
    else
    {
        ImGui::Text("GPU transparency: %.3f ms",timing.transparentMs);
        ImGui::Text("GPU composite: %.3f ms",timing.compositeMs);
    }
    ImGui::Separator();
    ImGui::TextWrapped("Weighted OIT: order independent, approximate. Intersections expose draw-sorting limits.");
    ImGui::TextWrapped("Gold foreground: opaque depth occluder. Gray rear plane: background reference.");
    if (capture_) ImGui::TextUnformatted("README capture / stationary until O");
    ImGui::End();
}

void App::HandleKey(WPARAM key)
{
    if (key >= '1' && key <= '3')
    {
        mode_ = static_cast<Mode>(key-'1');
        if (mode_ != Mode::WeightedOit) debugView_ = DebugView::Composite;
    }
    if (key == 'L' && laceValid_) settings_.laceExperiment = !settings_.laceExperiment;
    if (key == 'R') reverse_ = !reverse_;
    if (key == 'O') orbit_ = !orbit_;
    if (key == 'C') { closeUp_ = !closeUp_; if (closeUp_) scene_ = 1; }
    if (key == 'S') { scene_ = (scene_+1)%3; if (scene_ == 2) closeUp_ = false; }
}

void App::OnInputProcess(const Keyboard::State&,const Keyboard::KeyboardStateTracker&,
                         const Mouse::State&,const Mouse::ButtonStateTracker&)
{
    // Discrete shortcuts are handled in WM_KEYDOWN, so a drained down/up pair is never lost.
}

LRESULT CALLBACK App::WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_SIZE)
    {
        m_ClientWidth = LOWORD(lParam); m_ClientHeight = HIWORD(lParam);
        minimized_ = wParam == SIZE_MINIMIZED || m_ClientWidth == 0 || m_ClientHeight == 0;
        if (swapChain_ && !minimized_ && !failed_ && !Resize(m_ClientWidth,m_ClientHeight)) PostQuitMessage(1);
    }
    if (message == WM_KEYDOWN && !(lParam & (1LL << 30)) &&
        (!ImGui::GetCurrentContext() || !ImGui::GetIO().WantCaptureKeyboard)) HandleKey(wParam);
    return GameApp::WndProc(window,message,wParam,lParam);
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
    if (context_) { context_->ClearState(); context_->Flush(); }
    character_.reset();
    pipeline_ = OitPipeline{};
    backbuffer_.Reset(); surfaceVs_.Reset(); surfacePs_.Reset(); oitPs_.Reset(); layout_.Reset();
    constants_.Reset(); planeVertices_.Reset(); planeIndices_.Reset();
    cullBack_.Reset(); cullNone_.Reset(); sampler_.Reset();
    swapChain_.Reset(); context_.Reset(); device_.Reset();
}
