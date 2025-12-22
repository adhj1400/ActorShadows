#include "Globals.h"

namespace ActorShadowLimiter {

    std::map<uint32_t, uint32_t> g_originalLightTypes;
    std::map<uint32_t, bool> g_lastShadowStates;
    bool g_isReequipping = false;
    std::atomic<bool> g_pollThreadRunning{false};
    std::atomic<bool> g_shouldPoll{false};
    std::mutex g_lightModificationMutex;

}
