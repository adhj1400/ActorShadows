#pragma once

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {

    RE::TESObjectLIGH* GetEquippedLight(RE::PlayerCharacter* player);
    void ForceReequipLight(RE::PlayerCharacter* player);
    void AdjustLightPosition(RE::PlayerCharacter* player);

}
