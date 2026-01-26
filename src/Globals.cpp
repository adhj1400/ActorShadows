#include "Globals.h"

namespace ActorShadowLimiter {

    std::atomic<bool> g_pollThreadRunning{false};
    std::atomic<bool> g_shouldPoll{false};
    std::mutex g_lightModificationMutex;

}
