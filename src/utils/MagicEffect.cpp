#include "MagicEffect.h"

#include <cstdarg>

namespace ActorShadowLimiter {

    bool HasMagicEffect(RE::Actor* actor, RE::FormID effectFormID) {
        if (!actor) {
            return false;
        }

        auto* effect = RE::TESForm::LookupByID<RE::EffectSetting>(effectFormID);
        if (!effect) {
            return false;
        }

        auto* magicTarget = actor->GetMagicTarget();
        if (!magicTarget) {
            return false;
        }

        return magicTarget->HasMagicEffect(effect);
    }

}
