#pragma once

#include "RE/Skyrim.h"

namespace TorchShadowLimiter {

    RE::TESObjectLIGH* GetPlayerTorchBase(RE::PlayerCharacter* player);
    void ForceReequipTorch(RE::PlayerCharacter* player);
    void AdjustTorchLightPosition(RE::PlayerCharacter* player);

}  // namespace TorchShadowLimiter
