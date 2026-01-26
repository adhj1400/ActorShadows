#pragma once

namespace ActorShadowLimiter {
    void UpdateLightShadows(bool initialEquip = false);
    void StartShadowPollThread();
    void EnablePolling();
    void DisablePolling();
}
