#pragma once

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {

    RE::TESObjectLIGH* GetEquippedLight(RE::PlayerCharacter* player);
    void ResetEquippedLightToNoShadow(RE::PlayerCharacter* player);
    void ResetActiveSpellsToNoShadow(RE::PlayerCharacter* player);
    void ForceReequipLight(RE::PlayerCharacter* player);
    void AdjustHeldLightPosition(RE::PlayerCharacter* player);
    void AdjustSpellLightPosition(RE::PlayerCharacter* player, uint32_t spellFormId);
    std::vector<uint32_t> GetActiveConfiguredSpells(RE::PlayerCharacter* player);
    std::optional<uint32_t> GetActiveConfiguredLight(RE::PlayerCharacter* player);

}
