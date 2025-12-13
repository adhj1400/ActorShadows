#include "UpdateLogic.h"

#include <chrono>
#include <thread>

#include "Config.h"
#include "Globals.h"
#include "Helper.h"
#include "LightHelpers.h"
#include "SKSE/SKSE.h"
#include "ShadowCounter.h"
#include "TorchManager.h"

namespace TorchShadowLimiter {

    void UpdatePlayerLightShadows() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        auto* cell = player->GetParentCell();
        if (!cell) {
            return;
        }

        // Check if functionality is enabled for this cell type
        bool isExterior = cell->IsExteriorCell();
        if (isExterior && !g_config.enableExterior) {
            return;
        }
        if (!isExterior && !g_config.enableInterior) {
            return;
        }

        // Count shadow lights
        int shadowLightCount = CountNearbyShadowLights();

        // Decide on desired state
        bool wantShadows = (shadowLightCount < g_config.shadowLightLimit);

        // ============ Hand-Held Lights Logic ============
        for (uint32_t lightFormId : g_activeHandHeldLights) {
            auto* lightBase = GetPlayerTorchBase(player);
            if (lightBase && lightBase->GetFormID() == lightFormId) {
                // Only update if state changed from last known state
                bool lastState = g_lastShadowStates[lightFormId];
                if (wantShadows != lastState) {
                    DebugPrint("Hand-held light 0x%08X shadow state changed: %s (shadow lights: %d)", lightFormId,
                               wantShadows ? "ENABLE" : "DISABLE", shadowLightCount);

                    // Get current light type to remember original
                    uint8_t currentLightType = static_cast<uint8_t>(GetLightType(lightBase));

                    // Remember the original type if we're about to change it
                    if (g_originalLightTypes.find(lightFormId) == g_originalLightTypes.end()) {
                        g_originalLightTypes[lightFormId] = currentLightType;
                    }

                    // Modify the base form
                    uint8_t newType = wantShadows ? static_cast<uint8_t>(LightType::OmniShadow)
                                                  : static_cast<uint8_t>(LightType::OmniNS);
                    SetLightTypeNative(lightBase, newType);

                    // Force re-equip to update the reference
                    ForceReequipTorch(player);

                    // Adjust light position after re-equip
                    std::thread([lightFormId]() {
                        using namespace std::chrono_literals;
                        std::this_thread::sleep_for(200ms);

                        if (auto* tasks = SKSE::GetTaskInterface()) {
                            tasks->AddTask([lightFormId]() {
                                auto* pl = RE::PlayerCharacter::GetSingleton();
                                if (pl) {
                                    AdjustTorchLightPosition(pl);
                                }
                            });
                        }
                    }).detach();

                    g_lastShadowStates[lightFormId] = wantShadows;
                }
            }
        }

        // ============ Spell Lights Logic ============
        for (uint32_t spellFormId : g_activeSpells) {
            auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellFormId);
            if (!spell || spell->effects.size() == 0) continue;

            // Find the light-associated effect
            for (auto* effect : spell->effects) {
                if (!effect || !effect->baseEffect) continue;
                auto* assocForm = effect->baseEffect->data.associatedForm;
                if (!assocForm) continue;

                auto* lightBase = assocForm->As<RE::TESObjectLIGH>();
                if (!lightBase) continue;

                uint32_t effectFormId = effect->baseEffect->GetFormID();
                bool isActive = HasMagicEffect(player, effectFormId);

                if (isActive) {
                    // Only update if state changed from last known state
                    bool lastState = g_lastShadowStates[spellFormId];
                    if (wantShadows != lastState) {
                        DebugPrint("Spell 0x%08X shadow state changed: %s (shadow lights: %d)", spellFormId,
                                   wantShadows ? "ENABLE" : "DISABLE", shadowLightCount);

                        // Get current light type to remember original
                        uint8_t currentLightType = static_cast<uint8_t>(GetLightType(lightBase));

                        // Remember the original type
                        if (g_originalLightTypes.find(spellFormId) == g_originalLightTypes.end()) {
                            g_originalLightTypes[spellFormId] = currentLightType;
                        }

                        // Modify the base form
                        uint8_t newType = wantShadows ? static_cast<uint8_t>(LightType::OmniShadow)
                                                      : static_cast<uint8_t>(LightType::OmniNS);
                        SetLightTypeNative(lightBase, newType);

                        // Recast the spell
                        player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)
                            ->CastSpellImmediate(spell, false, player, 1.0f, false, 0.0f, nullptr);

                        // Restore base form after delay
                        std::thread([lightBase, spellFormId]() {
                            using namespace std::chrono_literals;
                            std::this_thread::sleep_for(1s);

                            if (auto* tasks = SKSE::GetTaskInterface()) {
                                tasks->AddTask([lightBase, spellFormId]() {
                                    uint32_t originalType = g_originalLightTypes[spellFormId];
                                    SetLightTypeNative(lightBase, originalType);
                                    DebugPrint("Restored spell 0x%08X base form to original type: %u", spellFormId,
                                               originalType);
                                });
                            }
                        }).detach();

                        g_lastShadowStates[spellFormId] = wantShadows;
                    }
                }
            }
        }
    }

    void StartShadowPollThread() {
        std::thread([]() {
            using namespace std::chrono_literals;

            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(g_config.pollIntervalSeconds));

                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([]() { UpdatePlayerLightShadows(); });
                }
            }
        }).detach();
        DebugPrint("Torch poll thread started (%ds interval)", g_config.pollIntervalSeconds);
    }

}
