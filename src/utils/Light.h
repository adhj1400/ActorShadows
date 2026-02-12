#pragma once

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {
    std::uint32_t GetLightType(const RE::TESObjectLIGH* a_light);
    void SetLightTypeNative(RE::TESObjectLIGH* a_light, bool withShadows);
    bool HasShadows(const RE::TESObjectLIGH* a_light);
}
