#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <d3d11.h>
#include <wrl/client.h>

#include "CoverageScene.h"

namespace Coverage40
{
struct Capabilities
{
    bool supports4x{};
    std::wstring reason;
};

struct GpuTimings
{
    bool available{}, valid{}, resolveApplicable{};
    Mode mode{ Mode::AlphaTest1x };
    UINT width{}, height{};
    double sceneMs{}, resolveMs{};
};

bool TryAdvanceTimingPublication(std::uint64_t submissionSerial,
    std::uint64_t& lastPublishedSerial);

class MsaaPipeline
{
public:
    bool Initialize(ID3D11Device* device, const std::wstring& shaderDirectory);
    bool Configure(ID3D11Device* device, UINT width, UINT height, Mode mode);
    void BeginFrame(ID3D11DeviceContext* context, const float clearColor[4]);
    void BeginSurface(ID3D11DeviceContext* context, SurfaceRule rule);
    void EndScene(ID3D11DeviceContext* context);
    void Resolve(ID3D11DeviceContext* context);
    void Present(ID3D11DeviceContext* context, ID3D11RenderTargetView* output,
                 float exposure);

    const Capabilities& Support() const { return support_; }
    const GpuTimings& Timings() const { return timings_; }
    Mode CurrentMode() const { return mode_; }
    UINT Width() const { return width_; }
    UINT Height() const { return height_; }
    UINT Samples() const { return SampleCount(mode_); }
    std::uint64_t MemoryBytes() const { return TargetBytes(width_, height_, Samples()); }
    ID3D11Texture2D* ColorTexture() const { return colorTexture_.Get(); }
    ID3D11Texture2D* DepthTexture() const { return depthTexture_.Get(); }
    ID3D11Texture2D* LinearTexture() const;

private:
    struct TimingSlot
    {
        Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
        Microsoft::WRL::ComPtr<ID3D11Query> sceneBegin;
        Microsoft::WRL::ComPtr<ID3D11Query> sceneEnd;
        Microsoft::WRL::ComPtr<ID3D11Query> resolveBegin;
        Microsoft::WRL::ComPtr<ID3D11Query> resolveEnd;
        bool inFlight{};
        bool sceneEnded{};
        std::uint64_t generation{};
        std::uint64_t submissionSerial{};
        Mode mode{ Mode::AlphaTest1x };
        UINT width{};
        UINT height{};
        bool resolveApplicable{};
    };

    static constexpr std::size_t kTimingSlotCount = 4;
    static constexpr std::size_t kNoTimingSlot = kTimingSlotCount;

    void InitializeTimingQueries(ID3D11Device* device);
    void DisableTimings();
    void PollTimings(ID3D11DeviceContext* context);
    void UpdateTimingConfiguration(Mode mode, UINT width, UINT height);

    Capabilities support_;
    GpuTimings timings_;
    std::array<TimingSlot, kTimingSlotCount> timingSlots_{};
    std::size_t activeTimingSlot_{ kNoTimingSlot };
    std::uint64_t timingGeneration_{};
    std::uint64_t nextTimingSubmissionSerial_{};
    std::uint64_t lastPublishedTimingSerial_{};
    Mode mode_{ Mode::AlphaTest1x };
    UINT width_{};
    UINT height_{};
    bool initialized_{};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> colorTexture_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> resolvedTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> colorTarget_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthTarget_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> linearView_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> opaqueBlend_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> alphaToCoverageBlend_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> alphaBlend_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthWrite_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthReadOnly_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthDisabled_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> presentRasterizer_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> presentVertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> presentPixelShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> presentConstants_;
};
}
