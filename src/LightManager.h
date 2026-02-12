#pragma once

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {
    RE::TESObjectLIGH* GetEquippedLight(RE::Actor* actor);

    void ForceReEquipLight(RE::Actor* actor, RE::TESObjectLIGH* light, bool withShadows);
    void ForceReEquipArmor(RE::Actor* actor, RE::TESObjectARMO* armor, bool withShadows);
    void ForceCastSpell(RE::Actor* actor, RE::SpellItem* spell, bool withShadows, bool skipIfNotActive = true);

    int CountNearbyShadowLights();

    std::vector<uint32_t> GetActiveConfiguredSpells(RE::Actor* actor);
    std::optional<uint32_t> GetActiveConfiguredLight(RE::Actor* actor);
    std::vector<uint32_t> GetActiveConfiguredEnchantedArmors(RE::Actor* actor);

    RE::TESObjectLIGH* GetLightFromEnchantedArmor(RE::TESObjectARMO* armor);
}
