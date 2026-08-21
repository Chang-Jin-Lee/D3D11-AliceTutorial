#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstdint>

enum class GpuPass : uint8_t { Shadow, Character, Outline, ToneMap, Count };

struct GpuTimings
{
    bool valid = false;
    double totalMs = 0.0;
    std::array<double, 4> passMs{};
};

class GpuProfiler final
{
public:
    bool Initialize(ID3D11Device* device);
    void BeginFrame(ID3D11DeviceContext* context);
    void BeginPass(ID3D11DeviceContext* context, GpuPass pass);
    void EndPass(ID3D11DeviceContext* context, GpuPass pass);
    void EndFrame(ID3D11DeviceContext* context);
    void Resolve(ID3D11DeviceContext* context);
    const GpuTimings& Latest() const;

private:
    static constexpr size_t kPassCount = static_cast<size_t>(GpuPass::Count);
    static constexpr size_t kQuerySlotCount = 4;
    static constexpr size_t kResolveDelay = 2;

    struct PassQueries
    {
        Microsoft::WRL::ComPtr<ID3D11Query> beginTimestamp;
        Microsoft::WRL::ComPtr<ID3D11Query> endTimestamp;
    };

    struct QuerySlot
    {
        Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
        std::array<PassQueries, kPassCount> passes;
        std::array<bool, kPassCount> completed{};
        bool submitted = false;
    };

    void Reset();
    static bool IsValidPass(GpuPass pass);

    std::array<QuerySlot, kQuerySlotCount> m_querySlots;
    std::array<bool, kPassCount> m_passOpen{};
    GpuTimings m_latest{};
    size_t m_writeSlot = 0;
    bool m_initialized = false;
    bool m_frameOpen = false;
};
