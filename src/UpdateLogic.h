#pragma once

namespace ActorShadowLimiter {
    void UpdateTrackedLights();
    void StartShadowPollThread();
    void EnablePolling();
    void DisablePolling();
    bool EvaluateActorAndScene(RE::Actor* actor);
}
