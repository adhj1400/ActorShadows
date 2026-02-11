#pragma once

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {
    void AdjustHeldLightPosition(RE::Actor* actor, uint32_t lightFormId);
    void AdjustSpellLightPosition(RE::Actor* actor, uint32_t spellFormId);
    void AdjustEnchantmentLightPosition(RE::Actor* actor, uint32_t armorFormId);
}
