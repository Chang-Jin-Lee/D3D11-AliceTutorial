#pragma once

#include <dxgi.h>

namespace Coverage40
{
enum class ResizeFailureStage
{
    ResizeBuffers,
    GetBuffer,
    BackbufferTarget,
    PipelineConfigure,
};

struct ResizeFailureDecision
{
    ResizeFailureStage stage;
    bool fatal;
    HRESULT reportedResult;
};

inline ResizeFailureDecision ClassifyResizeFailure(ResizeFailureStage stage,
    HRESULT operationResult, HRESULT deviceReason)
{
    const bool directDeviceLoss = operationResult == DXGI_ERROR_DEVICE_REMOVED ||
        operationResult == DXGI_ERROR_DEVICE_RESET;
    const bool fatal = directDeviceLoss || FAILED(deviceReason);
    return { stage, fatal,
        FAILED(deviceReason) ? deviceReason : operationResult };
}
}
