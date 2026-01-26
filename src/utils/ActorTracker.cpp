#include "ActorTracker.h"

namespace ActorShadowLimiter {

    void ActorTracker::AddActor(uint32_t actorFormId) {
        std::lock_guard<std::mutex> lock(mutex_);
        trackedActors_.insert(actorFormId);
    }

    void ActorTracker::RemoveActor(uint32_t actorFormId) {
        std::lock_guard<std::mutex> lock(mutex_);
        trackedActors_.erase(actorFormId);
        reequippingActors_.erase(actorFormId);  // Clean up re-equip state too
        shadowStates_.erase(actorFormId);       // Clean up shadow states too
    }

    bool ActorTracker::IsTracked(uint32_t actorFormId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return trackedActors_.find(actorFormId) != trackedActors_.end();
    }

    const std::set<uint32_t>& ActorTracker::GetTrackedActors() const {
        // Note: Returns reference, caller should lock if iterating
        return trackedActors_;
    }

    void ActorTracker::StartReequip(uint32_t actorFormId) {
        std::lock_guard<std::mutex> lock(mutex_);
        reequippingActors_.insert(actorFormId);
    }

    void ActorTracker::EndReequip(uint32_t actorFormId) {
        std::lock_guard<std::mutex> lock(mutex_);
        reequippingActors_.erase(actorFormId);
    }

    bool ActorTracker::IsReequipping(uint32_t actorFormId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reequippingActors_.find(actorFormId) != reequippingActors_.end();
    }

    void ActorTracker::SetShadowState(uint32_t actorFormId, uint32_t itemFormId, bool hasShadows) {
        std::lock_guard<std::mutex> lock(mutex_);
        shadowStates_[actorFormId][itemFormId] = hasShadows;
    }

    bool ActorTracker::GetShadowState(uint32_t actorFormId, uint32_t itemFormId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto actorIt = shadowStates_.find(actorFormId);
        if (actorIt == shadowStates_.end()) {
            return false;  // Actor not tracked, default to no shadows
        }
        auto itemIt = actorIt->second.find(itemFormId);
        return (itemIt != actorIt->second.end()) ? itemIt->second : false;
    }

    bool ActorTracker::HasShadowState(uint32_t actorFormId, uint32_t itemFormId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto actorIt = shadowStates_.find(actorFormId);
        if (actorIt == shadowStates_.end()) {
            return false;
        }
        return actorIt->second.find(itemFormId) != actorIt->second.end();
    }

}
