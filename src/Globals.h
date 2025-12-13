#pragma once

#include <mutex>

#include "RE/Skyrim.h"

namespace TorchShadowLimiter {

    using FLAGS = RE::TES_LIGHT_FLAGS;

    // Light type enumeration
    enum class LightType : std::uint8_t {
        HemiNS = 0,      // Hemisphere, no shadow
        HemiShadow = 1,  // Hemisphere with shadow
        OmniNS = 2,      // Omnidirectional, no shadow
        OmniShadow = 3,  // Omnidirectional with shadow
        Spotlight = 4,   // Spotlight
        SpotShadow = 5   // Spotlight with shadow
    };

    // FormIDs
    constexpr RE::FormID kCandlelightEffect = 0x0001EA6C;
    constexpr RE::FormID kCandlelightSpell = 0x00043324;

    // Global state
    extern bool g_lastShadowEnabled;
    extern bool g_lastCandlelightShadowEnabled;
    extern bool g_pollTorch;
    extern bool g_pollCandlelight;
    extern std::uint32_t g_originalTorchLightType;
    extern std::uint32_t g_originalCandlelightLightType;
    extern bool g_isReequippingTorch;

    // Mutex for thread-safe light modifications
    extern std::mutex g_lightModificationMutex;

}
