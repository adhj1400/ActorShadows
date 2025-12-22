#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <set>

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {

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

    // Global state - generalized for any lights/spells
    extern std::map<uint32_t, uint32_t> g_originalLightTypes;  // FormID -> original light type
    extern std::map<uint32_t, bool> g_lastShadowStates;        // FormID -> last shadow enabled state
    extern bool g_isReequipping;
    extern std::atomic<bool> g_pollThreadRunning;
    extern std::atomic<bool> g_shouldPoll;  // Controls whether polling should happen

    // Mutex for thread-safe light modifications
    extern std::mutex g_lightModificationMutex;

}
