#include "Globals.h"

namespace ActorShadowLimiter {

    std::set<uint32_t> g_activeHandHeldLights;
    std::set<uint32_t> g_activeSpells;
    std::map<uint32_t, uint32_t> g_originalLightTypes;
    std::map<uint32_t, bool> g_lastShadowStates;
    bool g_isReequipping = false;
    std::atomic<bool> g_pollThreadRunning{false};
    std::mutex g_lightModificationMutex;

}
