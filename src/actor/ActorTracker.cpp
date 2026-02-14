#include "ActorTracker.h"

#include <algorithm>

namespace ActorShadowLimiter {

    ActorTracker& ActorTracker::GetSingleton() {
        static ActorTracker instance;
        return instance;
    }

    TrackedActor* ActorTracker::GetOrCreateActor(uint32_t actorFormId) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Try to find existing actor
        auto it = trackedActors_.find(actorFormId);
        if (it != trackedActors_.end()) {
            return &it->second;
        }

        // Create new actor
        auto result = trackedActors_.emplace(actorFormId, TrackedActor(actorFormId));
        return &result.first->second;
    }

    void ActorTracker::AddActor(uint32_t actorFormId) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Only add if actor doesn't already exist
        if (trackedActors_.find(actorFormId) == trackedActors_.end()) {
            trackedActors_.emplace(actorFormId, TrackedActor(actorFormId));
        }
    }

    TrackedActor* ActorTracker::GetActor(uint32_t actorFormId) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = trackedActors_.find(actorFormId);
        if (it != trackedActors_.end()) {
            return &it->second;
        }

        return nullptr;
    }

    bool ActorTracker::HasActor(uint32_t actorFormId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return trackedActors_.find(actorFormId) != trackedActors_.end();
    }

    void ActorTracker::RemoveActor(uint32_t actorFormId) {
        std::lock_guard<std::mutex> lock(mutex_);
        trackedActors_.erase(actorFormId);
    }

    void ActorTracker::ClearAllActors() {
        std::lock_guard<std::mutex> lock(mutex_);
        trackedActors_.clear();
    }

    std::vector<uint32_t> ActorTracker::GetAllTrackedActorIds(bool sortByDistance, bool closestFirst) const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<uint32_t> actorIds;
        actorIds.reserve(trackedActors_.size());

        for (const auto& [actorFormId, actor] : trackedActors_) {
            actorIds.push_back(actorFormId);
        }

        // Sort by distance if requested
        if (sortByDistance) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                RE::NiPoint3 playerPos = player->GetPosition();

                std::sort(actorIds.begin(), actorIds.end(), [&playerPos, closestFirst](uint32_t a, uint32_t b) {
                    auto* actorA = RE::TESForm::LookupByID<RE::Actor>(a);
                    auto* actorB = RE::TESForm::LookupByID<RE::Actor>(b);

                    if (!actorA) return false;
                    if (!actorB) return true;

                    RE::NiPoint3 posA = actorA->GetPosition();
                    RE::NiPoint3 posB = actorB->GetPosition();

                    float distA = playerPos.GetSquaredDistance(posA);
                    float distB = playerPos.GetSquaredDistance(posB);

                    return closestFirst ? (distA < distB) : (distA > distB);
                });
            }
        }

        return actorIds;
    }

    size_t ActorTracker::GetTrackedActorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return trackedActors_.size();
    }

    size_t ActorTracker::GetTrackedActorsWithShadowsCount() const {
        std::lock_guard<std::mutex> lock(mutex_);

        size_t count = 0;
        for (const auto& [actorFormId, actor] : trackedActors_) {
            if (actor.HasAnyLightWithShadows()) {
                ++count;
            }
        }
        return count;
    }

    void ActorTracker::SetActorLightShadowState(uint32_t actorFormId, uint32_t lightFormId, bool hasShadows) {
        auto* actor = GetOrCreateActor(actorFormId);
        if (actor) {
            actor->SetLightShadowState(lightFormId, hasShadows);
        }
    }

    std::optional<bool> ActorTracker::GetActorLightShadowState(uint32_t actorFormId, uint32_t lightFormId) const {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = trackedActors_.find(actorFormId);
        if (it != trackedActors_.end()) {
            return it->second.GetLightShadowState(lightFormId);
        }

        return std::nullopt;
    }

    bool ActorTracker::ActorHasAnyLightWithShadows(uint32_t actorFormId) const {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = trackedActors_.find(actorFormId);
        if (it != trackedActors_.end()) {
            return it->second.HasAnyLightWithShadows();
        }

        return false;
    }

    bool ActorTracker::ContainsTrackedNpcs() const {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& [actorFormId, actor] : trackedActors_) {
            if (actorFormId != 0x14) {
                return true;
            }
        }

        return false;
    }
}
