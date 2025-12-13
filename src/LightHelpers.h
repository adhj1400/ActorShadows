#pragma once

#include "RE/Skyrim.h"

namespace TorchShadowLimiter {

    std::uint32_t GetLightType(const RE::TESObjectLIGH* a_light);
    void SetLightTypeNative(RE::TESObjectLIGH* a_light, std::uint32_t a_type);
}
