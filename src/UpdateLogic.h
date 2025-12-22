#pragma once

namespace ActorShadowLimiter {
    void UpdatePlayerLightShadows();
    void StartShadowPollThread();
    void EnablePolling();
    void DisablePolling();
}
