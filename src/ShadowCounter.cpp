#include "ShadowCounter.h"

#include "SKSE/SKSE.h"

namespace TorchShadowLimiter {

    int CountNearbyShadowLights() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return 0;
        }

        auto* cell = player->GetParentCell();
        if (!cell) {
            return 0;
        }

        RE::NiPoint3 playerPos = player->GetPosition();

        int shadowLightCount = 0;
        constexpr float searchRadius = 6000.0f;  // Skyrim units, larger range

        // Iterate references in the cell
        cell->ForEachReference([&](RE::TESObjectREFR& ref) {
            auto* lightBase = ref.GetBaseObject();
            if (!lightBase || lightBase->GetFormType() != RE::FormType::Light) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            // Grab the base record
            auto* lightBaseObj = lightBase->As<RE::TESObjectLIGH>();
            if (!lightBaseObj) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            // Check the light type
            uint8_t lightType = (uint8_t)lightBaseObj->data.flags.underlying();
            // Common bit flags:
            // 1 = Hemisphere Shadow Light
            // 3 = Omni Shadow Light (most shadow lights)
            // 5 = Spot Shadow Light

            bool isShadowLight = (lightType == 1 || lightType == 3 || lightType == 5);

            if (isShadowLight) {
                // Check distance
                RE::NiPoint3 lightPos = ref.GetPosition();
                float dx = lightPos.x - playerPos.x;
                float dy = lightPos.y - playerPos.y;
                float dz = lightPos.z - playerPos.z;
                float distSq = dx * dx + dy * dy + dz * dz;

                if (distSq <= (searchRadius * searchRadius)) {
                    ++shadowLightCount;
                }
            }

            return RE::BSContainer::ForEachResult::kContinue;
        });

        return shadowLightCount;
    }

}  // namespace TorchShadowLimiter
