#pragma once

#include <array>
#include <d3d11.h>
#include <string>
#include <wrl/client.h>

namespace Transparency39
{
    enum class Mode { AlphaTest, SortedBlend, WeightedOit };
    enum class DebugView { Composite, Accumulation, Revealage };
    struct GpuTimings { bool available; bool valid; double transparentMs; double compositeMs; };

    class OitPipeline
    {
    public:
        bool Initialize(ID3D11Device* device, const std::wstring& shaderDirectory);
        bool Resize(ID3D11Device* device, UINT width, UINT height);
        void BeginOpaque(ID3D11DeviceContext* context, const float clearColor[4]);
        void BeginTransparency(ID3D11DeviceContext* context, Mode mode);
        void EndTransparency(ID3D11DeviceContext* context);
        void Resolve(ID3D11DeviceContext* context);
        void Present(ID3D11DeviceContext* context, ID3D11RenderTargetView* output,
                     float exposure, DebugView debugView);
        ID3D11Texture2D* LinearTexture() const;
        ID3D11Texture2D* DepthTexture() const;
        ID3D11ShaderResourceView* AccumulationSRV() const;
        ID3D11ShaderResourceView* RevealageSRV() const;
        const GpuTimings& Timings() const;

    private:
        struct TimingSlot
        {
            Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
            Microsoft::WRL::ComPtr<ID3D11Query> transparentBegin;
            Microsoft::WRL::ComPtr<ID3D11Query> transparentEnd;
            Microsoft::WRL::ComPtr<ID3D11Query> compositeBegin;
            Microsoft::WRL::ComPtr<ID3D11Query> compositeEnd;
            bool inFlight{};
        };

        void PollTimings(ID3D11DeviceContext* context);
        void BeginTiming(ID3D11DeviceContext* context);
        void DrawFullscreen(ID3D11DeviceContext* context, ID3D11RenderTargetView* target,
                            ID3D11PixelShader* pixelShader);

        UINT width_{};
        UINT height_{};
        Mode mode_{ Mode::WeightedOit };
        Microsoft::WRL::ComPtr<ID3D11Texture2D> hdrTexture_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> hdrRtv_;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> hdrSrv_;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> accumulationTexture_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> accumulationRtv_;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> accumulationSrv_;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> revealageTexture_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> revealageRtv_;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> revealageSrv_;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> linearTexture_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> linearRtv_;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> linearSrv_;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthDsv_;
        Microsoft::WRL::ComPtr<ID3D11BlendState> opaqueBlend_;
        Microsoft::WRL::ComPtr<ID3D11BlendState> sortedBlend_;
        Microsoft::WRL::ComPtr<ID3D11BlendState> oitBlend_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthWrite_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthRead_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthDisabled_;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> fullscreenRasterizer_;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> fullscreenVertexShader_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> copyPixelShader_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> resolvePixelShader_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> presentCompositePixelShader_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> presentAccumulationPixelShader_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> presentRevealagePixelShader_;
        Microsoft::WRL::ComPtr<ID3D11Buffer> presentConstants_;
        static constexpr std::size_t kTimingSlotCount = 4;
        std::array<TimingSlot, kTimingSlotCount> timingSlots_{};
        std::size_t nextTimingSlot_{};
        int activeTimingSlot_{ -1 };
        bool transparentTimingEnded_{};
        GpuTimings timings_{};
    };
}
