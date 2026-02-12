#pragma once

namespace ActorShadowLimiter {
    void EnablePolling(int delayInSeconds = 4);
    void StartShadowPollThread();
    void StopShadowPollThread();

    /**
     * Main, non-actor-specific evaluation loop logic that runs continuously.
     * Evaluates the scene and each of the tracked actors, force re-equips where necessary.
     */
    void UpdateTrackedLights();

    /**
     * Evaluate the scene and returns wether shadows can be applied or not.
     */
    bool EvaluateActorAndScene(RE::Actor* actor);
}
