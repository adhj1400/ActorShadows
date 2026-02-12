#include "Light.h"

#include "../core/Globals.h"

namespace ActorShadowLimiter {
    std::uint32_t GetLightType(const RE::TESObjectLIGH* a_light) {
        if (!a_light) {
            return static_cast<std::uint32_t>(LightType::OmniNS);
        }

        const auto& flags = a_light->data.flags;

        if (flags.none(FLAGS::kHemiShadow) && flags.none(FLAGS::kOmniShadow) && flags.none(FLAGS::kSpotlight) &&
            flags.none(FLAGS::kSpotShadow)) {
            return static_cast<std::uint32_t>(LightType::OmniNS);
        }
        if (flags.any(FLAGS::kHemiShadow)) {
            return static_cast<std::uint32_t>(LightType::HemiShadow);
        }
        if (flags.any(FLAGS::kOmniShadow)) {
            return static_cast<std::uint32_t>(LightType::OmniShadow);
        }
        if (flags.any(FLAGS::kSpotlight)) {
            return static_cast<std::uint32_t>(LightType::Spotlight);
        }
        if (flags.any(FLAGS::kSpotShadow)) {
            return static_cast<std::uint32_t>(LightType::SpotShadow);
        }

        return static_cast<std::uint32_t>(LightType::OmniNS);
    }

    bool HasShadows(const RE::TESObjectLIGH* a_light) {
        if (!a_light) {
            return false;
        }

        const auto& flags = a_light->data.flags;
        return flags.any(FLAGS::kHemiShadow, FLAGS::kOmniShadow, FLAGS::kSpotShadow);
    }

    void SetLightTypeNative(RE::TESObjectLIGH* a_light, bool withShadows) {
        if (!a_light) {
            return;
        }

        // Thread-safe modification of base form
        std::lock_guard<std::mutex> lock(g_lightModificationMutex);

        auto& flags = a_light->data.flags;
        if (withShadows) {
            flags.set(FLAGS::kOmniShadow);
        } else {
            flags.reset(FLAGS::kHemiShadow, FLAGS::kOmniShadow, FLAGS::kSpotlight, FLAGS::kSpotShadow);
        }
    }

}
