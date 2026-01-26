#pragma once

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {
    namespace ShadowAllocator {
        // Structure to hold actor and their priority for shadow allocation
        struct ActorPriority {
            RE::Actor* actor;
            float distanceToPlayer;
            bool isPlayer;

            bool operator<(const ActorPriority& other) const {
                // Player always has highest priority
                if (isPlayer != other.isPlayer) {
                    return isPlayer > other.isPlayer;
                }
                // For NPCs, closer is better
                return distanceToPlayer < other.distanceToPlayer;
            }
        };

        // Allocate shadows among player and NPCs based on priority
        void AllocateShadows();
    }
}
