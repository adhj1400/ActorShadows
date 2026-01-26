#pragma once

#include <mutex>
#include <set>

namespace ActorShadowLimiter {

    /**
     * Tracks per-actor state for shadow management
     */
    class ActorTracker {
    public:
        static ActorTracker& GetSingleton() {
            static ActorTracker instance;
            return instance;
        }

        // Track which actors have configured items
        void AddActor(uint32_t actorFormId);
        void RemoveActor(uint32_t actorFormId);
        bool IsTracked(uint32_t actorFormId) const;
        const std::set<uint32_t>& GetTrackedActors() const;

        // Track which actors are currently re-equipping (to ignore their equip events)
        void StartReequip(uint32_t actorFormId);
        void EndReequip(uint32_t actorFormId);
        bool IsReequipping(uint32_t actorFormId) const;

        // Track shadow states per actor per item
        void SetShadowState(uint32_t actorFormId, uint32_t itemFormId, bool hasShadows);
        bool GetShadowState(uint32_t actorFormId, uint32_t itemFormId) const;
        bool HasShadowState(uint32_t actorFormId, uint32_t itemFormId) const;

    private:
        ActorTracker() = default;
        ~ActorTracker() = default;
        ActorTracker(const ActorTracker&) = delete;
        ActorTracker& operator=(const ActorTracker&) = delete;

        std::set<uint32_t> trackedActors_;                           // Actors with configured items
        std::set<uint32_t> reequippingActors_;                       // Actors currently in re-equip cycle
        std::map<uint32_t, std::map<uint32_t, bool>> shadowStates_;  // actorFormId -> (itemFormId -> hasShadows)
        mutable std::mutex mutex_;  // Thread safety for multi-threaded re-equip callbacks
    };

}
