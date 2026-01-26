#pragma once

#include "RE/Skyrim.h"
#include "utils/ActorLight.h"

namespace ActorShadowLimiter {
    // Actor-agnostic functions (work with any actor including player)
    void ResetEquippedLightToNoShadow(RE::Actor* actor);
    void ResetActiveSpellsToNoShadow(RE::Actor* actor);
    void ResetActiveEnchantedArmorsToNoShadow(RE::Actor* actor);
    void ForceReequipLight(RE::Actor* actor);
    void ForceReequipArmor(RE::Actor* actor, RE::TESObjectARMO* armor);
    void AdjustHeldLightPosition(RE::Actor* actor);
    void AdjustSpellLightPosition(RE::Actor* actor, uint32_t spellFormId);
    void AdjustEnchantmentLightPosition(RE::Actor* actor, uint32_t armorFormId);
    void CleanupDuplicateLightNodes(RE::Actor* actor);
    RE::TESObjectLIGH* GetLightFromEnchantedArmor(RE::TESObjectARMO* armor);

    // Player-specific convenience overloads (for backward compatibility)
    inline void ResetEquippedLightToNoShadow(RE::PlayerCharacter* player) {
        ResetEquippedLightToNoShadow(static_cast<RE::Actor*>(player));
    }
    inline void ResetActiveSpellsToNoShadow(RE::PlayerCharacter* player) {
        ResetActiveSpellsToNoShadow(static_cast<RE::Actor*>(player));
    }
    inline void ResetActiveEnchantedArmorsToNoShadow(RE::PlayerCharacter* player) {
        ResetActiveEnchantedArmorsToNoShadow(static_cast<RE::Actor*>(player));
    }
    inline void ForceReequipLight(RE::PlayerCharacter* player) { ForceReequipLight(static_cast<RE::Actor*>(player)); }
    inline void ForceReequipArmor(RE::PlayerCharacter* player, RE::TESObjectARMO* armor) {
        ForceReequipArmor(static_cast<RE::Actor*>(player), armor);
    }
    inline void AdjustHeldLightPosition(RE::PlayerCharacter* player) {
        AdjustHeldLightPosition(static_cast<RE::Actor*>(player));
    }
    inline void AdjustSpellLightPosition(RE::PlayerCharacter* player, uint32_t spellFormId) {
        AdjustSpellLightPosition(static_cast<RE::Actor*>(player), spellFormId);
    }
    inline void AdjustEnchantmentLightPosition(RE::PlayerCharacter* player, uint32_t armorFormId) {
        AdjustEnchantmentLightPosition(static_cast<RE::Actor*>(player), armorFormId);
    }
}
