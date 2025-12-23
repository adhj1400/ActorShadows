#include "Light.h"

#include "../Globals.h"

namespace ActorShadowLimiter {

    std::uint32_t GetLightType(const RE::TESObjectLIGH* a_light) {
        if (!a_light) {
            return static_cast<std::uint32_t>(LightType::HemiNS);
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

        return static_cast<std::uint32_t>(LightType::HemiNS);
    }

    bool HasShadows(const RE::TESObjectLIGH* a_light) {
        if (!a_light) {
            return false;
        }

        const auto& flags = a_light->data.flags;
        return flags.any(FLAGS::kHemiShadow, FLAGS::kOmniShadow, FLAGS::kSpotShadow);
    }

    bool IsOfShadowType(std::uint32_t a_type) {
        return a_type == static_cast<std::uint32_t>(LightType::HemiShadow) ||
               a_type == static_cast<std::uint32_t>(LightType::OmniShadow) ||
               a_type == static_cast<std::uint32_t>(LightType::SpotShadow);
    }

    void SetLightTypeNative(RE::TESObjectLIGH* a_light, std::uint32_t a_type) {
        if (!a_light) {
            return;
        }

        // Thread-safe modification of base form
        std::lock_guard<std::mutex> lock(g_lightModificationMutex);

        auto& flags = a_light->data.flags;
        switch (a_type) {
            case static_cast<std::uint32_t>(LightType::HemiShadow):
                flags.reset(FLAGS::kOmniShadow, FLAGS::kSpotlight, FLAGS::kSpotShadow);
                flags.set(FLAGS::kHemiShadow);
                break;
            case static_cast<std::uint32_t>(LightType::OmniNS):
                flags.reset(FLAGS::kHemiShadow, FLAGS::kOmniShadow, FLAGS::kSpotlight, FLAGS::kSpotShadow);
                break;
            case static_cast<std::uint32_t>(LightType::OmniShadow):
                flags.reset(FLAGS::kHemiShadow, FLAGS::kSpotlight, FLAGS::kSpotShadow);
                flags.set(FLAGS::kOmniShadow);
                break;
            case static_cast<std::uint32_t>(LightType::Spotlight):
                flags.reset(FLAGS::kHemiShadow, FLAGS::kOmniShadow, FLAGS::kSpotShadow);
                flags.set(FLAGS::kSpotlight);
                break;
            case static_cast<std::uint32_t>(LightType::SpotShadow):
                flags.reset(FLAGS::kHemiShadow, FLAGS::kOmniShadow, FLAGS::kSpotlight);
                flags.set(FLAGS::kSpotShadow);
                break;
            default:
                break;
        }
    }

}
