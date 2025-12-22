#include "Helpers.h"

namespace ActorShadowLimiter {

    bool IsPlayer(RE::TESObjectREFR* ref) {
        if (!ref) {
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return false;
        }

        return ref->GetHandle() == player->GetHandle();
    }

}
