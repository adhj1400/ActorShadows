#pragma once

#include <cstdint>
#include <optional>

namespace ActorShadowLimiter {

    class TrackedActor {
    public:
        // Constructor
        explicit TrackedActor(uint32_t actorFormId);

        // Actor ID
        uint32_t GetActorFormId() const;

        // Light tracking
        void SetTrackedLight(uint32_t lightFormId);
        void SetLightShadowState(uint32_t lightFormId, bool hasShadows);
        bool GetLightShadowState(uint32_t lightFormId) const;
        bool HasAnyLightWithShadows() const;
        void RemoveLight(uint32_t lightFormId);
        void ClearTrackedLight();

        std::optional<uint32_t> GetTrackedLight() const;
        bool HasTrackedLight() const;

        // Re-equipping state
        bool IsReEquipping() const;
        void SetReEquipping(bool reEquipping);

    private:
        uint32_t actorFormId_;
        std::optional<uint32_t> trackedLightFormId_;
        bool hasShadows_ = false;
        bool isReEquipping_ = false;
    };

}
