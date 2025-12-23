#pragma once

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {
    std::uint32_t GetLightType(const RE::TESObjectLIGH* a_light);
    void SetLightTypeNative(RE::TESObjectLIGH* a_light, std::uint32_t a_type);
    bool HasShadows(const RE::TESObjectLIGH* a_light);
    bool IsOfShadowType(std::uint32_t a_type);
}
