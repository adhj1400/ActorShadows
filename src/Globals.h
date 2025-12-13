#pragma once

#include <map>
#include <mutex>
#include <set>

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

    // Global state - generalized for any lights/spells
    extern std::set<uint32_t> g_activeHandHeldLights;          // Currently equipped hand-held lights (by FormID)
    extern std::set<uint32_t> g_activeSpells;                  // Currently active spell effects (by FormID)
    extern std::map<uint32_t, uint32_t> g_originalLightTypes;  // FormID -> original light type
    extern std::map<uint32_t, bool> g_lastShadowStates;        // FormID -> last shadow enabled state
    extern bool g_isReequipping;

    // Mutex for thread-safe light modifications
    extern std::mutex g_lightModificationMutex;

}
