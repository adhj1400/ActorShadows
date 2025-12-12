#include "Globals.h"

namespace TorchShadowLimiter {

    bool g_lastShadowEnabled = false;
    bool g_lastCandlelightShadowEnabled = false;
    bool g_pollTorch = false;
    bool g_pollCandlelight = false;
    std::uint32_t g_originalTorchLightType = 255;
    std::uint32_t g_originalCandlelightLightType = 255;
    bool g_isReequippingTorch = false;
    std::mutex g_lightModificationMutex;
}  // namespace TorchShadowLimiter
