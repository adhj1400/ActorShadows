#pragma once

namespace ActorShadowLimiter {
    void UpdateTrackedLights();
    void StartShadowPollThread();
    void EnablePolling(int delayInSeconds = 4);
    void DisablePolling();
    bool EvaluateActorAndScene(RE::Actor* actor);
}
