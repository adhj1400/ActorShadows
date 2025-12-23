#pragma once

namespace ActorShadowLimiter {
    void UpdatePlayerLightShadows(bool initialEquip = false);
    void StartShadowPollThread();
    void EnablePolling();
    void DisablePolling();
}
