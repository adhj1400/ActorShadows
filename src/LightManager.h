#pragma once

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {
    RE::TESObjectLIGH* GetEquippedLight(RE::PlayerCharacter* player);
    void ResetEquippedLightToNoShadow(RE::PlayerCharacter* player);
    void ResetActiveSpellsToNoShadow(RE::PlayerCharacter* player);
    void ResetActiveEnchantedArmorsToNoShadow(RE::PlayerCharacter* player);
    void ForceReequipLight(RE::PlayerCharacter* player);
    void ForceReequipArmor(RE::PlayerCharacter* player, RE::TESObjectARMO* armor);
    void AdjustHeldLightPosition(RE::PlayerCharacter* player);
    void AdjustSpellLightPosition(RE::PlayerCharacter* player, uint32_t spellFormId);
    void AdjustEnchantmentLightPosition(RE::PlayerCharacter* player, uint32_t armorFormId);
    std::vector<uint32_t> GetActiveConfiguredSpells(RE::PlayerCharacter* player);
    std::optional<uint32_t> GetActiveConfiguredLight(RE::PlayerCharacter* player);
    std::vector<uint32_t> GetActiveConfiguredEnchantedArmors(RE::PlayerCharacter* player);
    RE::TESObjectLIGH* GetLightFromEnchantedArmor(RE::TESObjectARMO* armor);
}
