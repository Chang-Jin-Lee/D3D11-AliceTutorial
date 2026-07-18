#pragma once

#include <cstdlib>
#include <cstring>

namespace ReadmeCapture
{
    inline bool IsEnabled()
    {
        static const bool enabled = []()
        {
#if defined(_MSC_VER)
            char* value = nullptr;
            size_t valueLength = 0;
            if (_dupenv_s(&value, &valueLength, "DX11_README_CAPTURE") != 0 || !value)
                return false;

            const bool captureEnabled = value[0] != '\0' && std::strcmp(value, "0") != 0;
            std::free(value);
            return captureEnabled;
#else
            const char* value = std::getenv("DX11_README_CAPTURE");
            return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
#endif
        }();
        return enabled;
    }
}
