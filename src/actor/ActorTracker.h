#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "TrackedActor.h"

namespace ActorShadowLimiter {

    class ActorTracker {
    public:
        // Singleton access
        static ActorTracker& GetSingleton();

        // Actor management
        TrackedActor* GetOrCreateActor(uint32_t actorFormId);
        TrackedActor* GetActor(uint32_t actorFormId);
        void AddActor(uint32_t actorFormId);
        bool HasActor(uint32_t actorFormId) const;
        void RemoveActor(uint32_t actorFormId);
        void ClearAllActors();
        bool ContainsTrackedNpcs() const;

        // Get all tracked actors
        std::vector<uint32_t> GetAllTrackedActorIds(bool sortByDistance = false, bool closestFirst = true) const;
        size_t GetTrackedActorCount() const;
        size_t GetTrackedActorsWithShadowsCount() const;

        // Light management shortcuts
        void SetActorLightShadowState(uint32_t actorFormId, uint32_t lightFormId, bool hasShadows);
        std::optional<bool> GetActorLightShadowState(uint32_t actorFormId, uint32_t lightFormId) const;
        bool ActorHasAnyLightWithShadows(uint32_t actorFormId) const;

        // Prevent copying
        ActorTracker(const ActorTracker&) = delete;
        ActorTracker& operator=(const ActorTracker&) = delete;

    private:
        ActorTracker() = default;

        std::map<uint32_t, TrackedActor> trackedActors_;
        mutable std::mutex mutex_;
    };

}
