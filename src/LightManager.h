#pragma once

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {
    RE::TESObjectLIGH* GetEquippedLight(RE::Actor* actor);
    void ResetEquippedLightToNoShadow(RE::Actor* actor);
    void ResetActiveSpellsToNoShadow(RE::Actor* actor);
    void ResetActiveEnchantedArmorsToNoShadow(RE::Actor* actor);
    void ForceReequipLight(RE::Actor* actor, RE::TESObjectLIGH* light, bool withShadows);
    void ForceReequipArmor(RE::Actor* actor, RE::TESObjectARMO* armor);
    void AdjustHeldLightPosition(RE::Actor* actor);
    void AdjustSpellLightPosition(RE::Actor* actor, uint32_t spellFormId);
    void AdjustEnchantmentLightPosition(RE::Actor* actor, uint32_t armorFormId);
    int CountNearbyShadowLights();
    std::vector<uint32_t> GetActiveConfiguredSpells(RE::Actor* actor);
    std::optional<uint32_t> GetActiveConfiguredLight(RE::Actor* actor);
    std::vector<uint32_t> GetActiveConfiguredEnchantedArmors(RE::Actor* actor);
    RE::TESObjectLIGH* GetLightFromEnchantedArmor(RE::TESObjectARMO* armor);
}
