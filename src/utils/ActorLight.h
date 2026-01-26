#pragma once

#include <optional>
#include <vector>

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {
    // Get equipped light for any actor
    RE::TESObjectLIGH* GetEquippedLight(RE::Actor* actor);

    // Remove ExtraLight from actor's equipped torch to clean up duplicates
    void RemoveEquippedLightExtraData(RE::Actor* actor);

    // Get active configured spells for any actor
    std::vector<uint32_t> GetActiveConfiguredSpells(RE::Actor* actor);

    // Get active configured enchanted armors for any actor
    std::vector<uint32_t> GetActiveConfiguredEnchantedArmors(RE::Actor* actor);

    // Get active configured light FormID if any
    std::optional<uint32_t> GetActiveConfiguredLight(RE::Actor* actor);

    // Check if actor has any configured items
    bool HasConfiguredItems(RE::Actor* actor);
}
