#include "ShadowCounter.h"

#include "../Config.h"
#include "Console.h"
#include "SKSE/SKSE.h"

namespace ActorShadowLimiter {

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
        float maxSearchRadius = g_config.maxSearchRadius;

        // Get shadow distance from game settings
        float shadowDistance = 3000.0f;  // Default fallback
        bool isInterior = cell->IsInteriorCell();

        auto* gameSettingCollection = RE::GameSettingCollection::GetSingleton();
        if (gameSettingCollection) {
            auto* setting =
                gameSettingCollection->GetSetting(isInterior ? "fInteriorShadowDistance" : "fShadowDistance");
            if (setting) {
                shadowDistance = setting->GetFloat();
            }
        }

        // Count shadow lights in a cell
        auto countInCell = [&](RE::TESObjectCELL* targetCell) {
            if (!targetCell) {
                return;
            }

            targetCell->ForEachReference([&](RE::TESObjectREFR* ref) {
                if (!ref) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                auto* lightBase = ref->GetBaseObject();
                if (!lightBase || lightBase->GetFormType() != RE::FormType::Light) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                // Grab the base record
                auto* lightBaseObj = lightBase->As<RE::TESObjectLIGH>();
                if (!lightBaseObj) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                // Check if it has shadow flags
                const auto& flags = lightBaseObj->data.flags;
                bool isShadowLight = flags.any(RE::TES_LIGHT_FLAGS::kHemiShadow) ||
                                     flags.any(RE::TES_LIGHT_FLAGS::kOmniShadow) ||
                                     flags.any(RE::TES_LIGHT_FLAGS::kSpotShadow);

                if (isShadowLight) {
                    // Check distance from player
                    RE::NiPoint3 lightPos = ref->GetPosition();
                    float dx = lightPos.x - playerPos.x;
                    float dy = lightPos.y - playerPos.y;
                    float dz = lightPos.z - playerPos.z;
                    float distSq = dx * dx + dy * dy + dz * dz;

                    // Limit search area
                    if (distSq <= (maxSearchRadius * maxSearchRadius)) {
                        float distance = std::sqrt(distSq);
                        bool isDisabled = ref->IsDisabled();
                        bool isDeleted = ref->IsDeleted();
                        bool isInitiallyDisabled = ref->GetFormFlags() & 0x800;  // Initially disabled flag
                        bool hasLightAttached =
                            ref->extraList.HasType(RE::ExtraDataType::kLight);  // Required by LightPlacer

                        // Get light properties that LightPlacer might be modifying
                        int32_t radius = lightBaseObj->data.radius;

                        // Check for ExtraRadius override (XRDS)
                        int32_t actualRadius = radius;
                        auto* extraRadius = ref->extraList.GetByType<RE::ExtraRadius>();
                        if (extraRadius) {
                            actualRadius = static_cast<int32_t>(extraRadius->radius);
                        }

                        // Check if the light is actually rendering by examining the NiLight node
                        bool isRendering = false;
                        if (hasLightAttached) {
                            auto* extraLight = ref->extraList.GetByType<RE::ExtraLight>();
                            if (extraLight && extraLight->lightData && extraLight->lightData->light) {
                                auto* niLight = extraLight->lightData->light.get();
                                // A light is rendering if the node exists and is not culled
                                isRendering = niLight != nullptr;
                            }
                        }

                        // Effective shadow distance: light radius + game's shadow distance setting
                        float effectiveShadowDistance = actualRadius + shadowDistance;
                        bool withinEffectiveShadowDist = distance <= effectiveShadowDistance;

                        DebugPrint("SCAN",
                                   "Base: 0x%08X, Ref: 0x%08X, Radius: %d, "
                                   "ShadowDist: %.1f, EffectiveDist: %.1f, "
                                   "Disabled: %s, HasLight: %s",
                                   lightBaseObj->GetFormID(), ref->GetFormID(), actualRadius, shadowDistance,
                                   effectiveShadowDistance, isDisabled ? "YES" : "NO", hasLightAttached ? "YES" : "NO");

                        // Only count lights within effective shadow distance and actually emitting
                        // LightPlacer removes lights by not attaching ExtraLight data
                        if (!isDisabled && !isDeleted && !isInitiallyDisabled && hasLightAttached && actualRadius > 0 &&
                            isRendering && withinEffectiveShadowDist) {
                            ++shadowLightCount;
                        }
                    }
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });
        };

        // Count lights in the current cell
        countInCell(cell);
        auto isExterior = cell->IsExteriorCell();

        // If in exterior, also check neighboring cells
        if (isExterior) {
            auto* tes = RE::TES::GetSingleton();
            if (tes && tes->interiorCell == nullptr) {
                // Get the grid of loaded cells around the player
                auto* gridCells = tes->gridCells;
                if (gridCells) {
                    for (uint32_t x = 0; x < gridCells->length; ++x) {
                        for (uint32_t y = 0; y < gridCells->length; ++y) {
                            auto* gridCell = gridCells->GetCell(x, y);
                            if (gridCell && gridCell != cell) {
                                countInCell(gridCell);
                            }
                        }
                    }
                }
            }
        }

        DebugPrint("SCAN", "There are %d lights too close to the player, Type: %s", shadowLightCount,
                   isExterior ? "EXTERIOR" : "INTERIOR");
        return shadowLightCount;
    }

}
