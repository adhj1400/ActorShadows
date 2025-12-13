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
        float searchRadius = g_config.searchRadius;

        // Lambda to count shadow lights in a cell
        auto countInCell = [&](RE::TESObjectCELL* targetCell) {
            if (!targetCell) {
                return;
            }

            targetCell->ForEachReference([&](RE::TESObjectREFR& ref) {
                auto* lightBase = ref.GetBaseObject();
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
        };

        // Count lights in the current cell
        countInCell(cell);

        // If in exterior, also check neighboring cells
        if (cell->IsExteriorCell()) {
            DebugPrint("Exterior cell detected, checking grid cells");
            auto* tes = RE::TES::GetSingleton();
            if (tes && tes->interiorCell == nullptr) {
                // Get the grid of loaded cells around the player
                auto* gridCells = tes->gridCells;
                if (gridCells) {
                    DebugPrint("Grid cells available, length: %u", gridCells->length);
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

        DebugPrint("Total shadow lights found: %d", shadowLightCount);
        return shadowLightCount;
    }

}
