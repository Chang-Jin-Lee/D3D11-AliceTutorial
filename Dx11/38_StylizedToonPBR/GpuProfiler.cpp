#include "GpuProfiler.h"

#include <algorithm>

bool GpuProfiler::Initialize(ID3D11Device* device)
{
    Reset();
    if (device == nullptr)
    {
        return false;
    }

    D3D11_QUERY_DESC queryDescription{};
    queryDescription.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

    for (auto& slot : m_querySlots)
    {
        if (FAILED(device->CreateQuery(&queryDescription, slot.disjoint.GetAddressOf())))
        {
            Reset();
            return false;
        }

        queryDescription.Query = D3D11_QUERY_TIMESTAMP;
        for (auto& pass : slot.passes)
        {
            if (FAILED(device->CreateQuery(&queryDescription, pass.beginTimestamp.GetAddressOf())) ||
                FAILED(device->CreateQuery(&queryDescription, pass.endTimestamp.GetAddressOf())))
            {
                Reset();
                return false;
            }
        }

        queryDescription.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    }

    m_initialized = true;
    return true;
}

void GpuProfiler::BeginFrame(ID3D11DeviceContext* context)
{
    if (!m_initialized || context == nullptr || m_frameOpen)
    {
        return;
    }

    QuerySlot& slot = m_querySlots[m_writeSlot];
    slot.submitted = false;
    slot.completed.fill(false);
    m_passOpen.fill(false);

    context->Begin(slot.disjoint.Get());
    m_frameOpen = true;
}

void GpuProfiler::BeginPass(ID3D11DeviceContext* context, GpuPass pass)
{
    if (!m_initialized || context == nullptr || !m_frameOpen || !IsValidPass(pass))
    {
        return;
    }

    const size_t passIndex = static_cast<size_t>(pass);
    if (m_passOpen[passIndex] || m_querySlots[m_writeSlot].completed[passIndex])
    {
        return;
    }

    context->End(m_querySlots[m_writeSlot].passes[passIndex].beginTimestamp.Get());
    m_passOpen[passIndex] = true;
}

void GpuProfiler::EndPass(ID3D11DeviceContext* context, GpuPass pass)
{
    if (!m_initialized || context == nullptr || !m_frameOpen || !IsValidPass(pass))
    {
        return;
    }

    const size_t passIndex = static_cast<size_t>(pass);
    if (!m_passOpen[passIndex])
    {
        return;
    }

    QuerySlot& slot = m_querySlots[m_writeSlot];
    context->End(slot.passes[passIndex].endTimestamp.Get());
    m_passOpen[passIndex] = false;
    slot.completed[passIndex] = true;
}

void GpuProfiler::EndFrame(ID3D11DeviceContext* context)
{
    if (!m_initialized || context == nullptr || !m_frameOpen)
    {
        return;
    }

    QuerySlot& slot = m_querySlots[m_writeSlot];
    for (size_t passIndex = 0; passIndex < kPassCount; ++passIndex)
    {
        if (m_passOpen[passIndex])
        {
            context->End(slot.passes[passIndex].endTimestamp.Get());
            m_passOpen[passIndex] = false;
            slot.completed[passIndex] = true;
        }
    }

    context->End(slot.disjoint.Get());
    slot.submitted = false;
    if (std::all_of(slot.completed.begin(), slot.completed.end(), [](bool completed) { return completed; }))
    {
        slot.submitted = true;
    }

    m_frameOpen = false;
    m_writeSlot = (m_writeSlot + 1) % kQuerySlotCount;
}

void GpuProfiler::Resolve(ID3D11DeviceContext* context)
{
    if (!m_initialized || context == nullptr)
    {
        return;
    }

    const size_t resolveSlot = (m_writeSlot + kQuerySlotCount - kResolveDelay) % kQuerySlotCount;
    QuerySlot& slot = m_querySlots[resolveSlot];
    if (!slot.submitted)
    {
        return;
    }

    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData{};
    if (context->GetData(
            slot.disjoint.Get(),
            &disjointData,
            sizeof(disjointData),
            D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
    {
        return;
    }
    if (disjointData.Disjoint || disjointData.Frequency == 0)
    {
        slot.submitted = false;
        return;
    }

    std::array<std::uint64_t, kPassCount> beginTicks{};
    std::array<std::uint64_t, kPassCount> endTicks{};
    for (size_t passIndex = 0; passIndex < kPassCount; ++passIndex)
    {
        if (context->GetData(
                slot.passes[passIndex].beginTimestamp.Get(),
                &beginTicks[passIndex],
                sizeof(beginTicks[passIndex]),
                D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
        {
            return;
        }
        if (context->GetData(
                slot.passes[passIndex].endTimestamp.Get(),
                &endTicks[passIndex],
                sizeof(endTicks[passIndex]),
                D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
        {
            return;
        }
    }

    GpuTimings resolved{};
    resolved.valid = true;
    const double millisecondsPerTick = 1000.0 / static_cast<double>(disjointData.Frequency);
    for (size_t passIndex = 0; passIndex < kPassCount; ++passIndex)
    {
        if (endTicks[passIndex] < beginTicks[passIndex])
        {
            slot.submitted = false;
            return;
        }

        resolved.passMs[passIndex] =
            static_cast<double>(endTicks[passIndex] - beginTicks[passIndex]) * millisecondsPerTick;
        resolved.totalMs += resolved.passMs[passIndex];
    }

    m_latest = resolved;
    slot.submitted = false;
}

const GpuTimings& GpuProfiler::Latest() const
{
    return m_latest;
}

void GpuProfiler::Reset()
{
    for (auto& slot : m_querySlots)
    {
        slot.disjoint.Reset();
        for (auto& pass : slot.passes)
        {
            pass.beginTimestamp.Reset();
            pass.endTimestamp.Reset();
        }
        slot.completed.fill(false);
        slot.submitted = false;
    }

    m_passOpen.fill(false);
    m_latest = {};
    m_writeSlot = 0;
    m_initialized = false;
    m_frameOpen = false;
}

bool GpuProfiler::IsValidPass(GpuPass pass)
{
    return static_cast<size_t>(pass) < kPassCount;
}
