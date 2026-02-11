#include "TrackedActor.h"

namespace ActorShadowLimiter {

    TrackedActor::TrackedActor(uint32_t actorFormId) : actorFormId_(actorFormId) {}

    uint32_t TrackedActor::GetActorFormId() const { return actorFormId_; }

    void TrackedActor::SetTrackedLight(uint32_t lightFormId) { trackedLightFormId_ = lightFormId; }

    void TrackedActor::SetLightShadowState(uint32_t lightFormId, bool hasShadows) {
        trackedLightFormId_ = lightFormId;
        hasShadows_ = hasShadows;
    }

    bool TrackedActor::GetLightShadowState(uint32_t lightFormId) const {
        if (trackedLightFormId_.has_value() && trackedLightFormId_.value() == lightFormId) {
            return hasShadows_;
        }
        return false;
    }

    bool TrackedActor::HasAnyLightWithShadows() const { return trackedLightFormId_.has_value() && hasShadows_; }

    void TrackedActor::RemoveLight(uint32_t lightFormId) {
        if (trackedLightFormId_.has_value() && trackedLightFormId_.value() == lightFormId) {
            trackedLightFormId_.reset();
            hasShadows_ = false;
        }
    }

    void TrackedActor::ClearTrackedLight() {
        trackedLightFormId_.reset();
        hasShadows_ = false;
    }

    std::optional<uint32_t> TrackedActor::GetTrackedLight() const { return trackedLightFormId_; }

    bool TrackedActor::HasTrackedLight() const { return trackedLightFormId_.has_value(); }

    bool TrackedActor::IsReEquipping() const { return isReEquipping_; }

    void TrackedActor::SetReEquipping(bool reEquipping) { isReEquipping_ = reEquipping; }

}
