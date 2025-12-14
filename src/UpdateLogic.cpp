#include "UpdateLogic.h"

#include <chrono>
#include <thread>

#include "Config.h"
#include "Globals.h"
#include "LightManager.h"
#include "SKSE/SKSE.h"
#include "utils/Console.h"
#include "utils/Light.h"
#include "utils/MagicEffect.h"
#include "utils/ShadowCounter.h"

namespace ActorShadowLimiter {

    namespace {
        // Scan for any configured spells currently active on the player
        std::vector<uint32_t> GetActiveConfiguredSpells(RE::PlayerCharacter* player) {
            std::vector<uint32_t> activeSpells;

            for (const auto& spellConfig : g_config.spells) {
                auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellConfig.formId);
                if (!spell || spell->effects.size() == 0) continue;

                // Check if any of this spell's effects are active
                for (auto* effect : spell->effects) {
                    if (!effect || !effect->baseEffect) continue;

                    if (HasMagicEffect(player, effect->baseEffect->GetFormID())) {
                        activeSpells.push_back(spellConfig.formId);
                        break;  // Found this spell active, move to next spell
                    }
                }
            }

            return activeSpells;
        }

        // Check if player has a configured hand-held light equipped
        std::optional<uint32_t> GetActiveConfiguredLight(RE::PlayerCharacter* player) {
            auto* lightBase = GetEquippedLight(player);
            if (!lightBase) return std::nullopt;

            uint32_t lightFormId = lightBase->GetFormID();

            // Check if this light is in our configuration
            for (const auto& config : g_config.handHeldLights) {
                if (config.formId == lightFormId) {
                    return lightFormId;
                }
            }

            return std::nullopt;
        }
    }

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

        // Scan for active configured spells and lights
        auto activeSpells = GetActiveConfiguredSpells(player);
        auto activeLight = GetActiveConfiguredLight(player);

        // If no configured items are active, stop the polling thread
        if (!activeLight.has_value() && activeSpells.empty()) {
            if (g_pollThreadRunning) {
                DebugPrint("No configured lights or spells active - stopping shadow polling thread");
                g_pollThreadRunning = false;
            }
            return;
        }

        // ============ Hand-Held Lights Logic ============
        if (activeLight.has_value() && activeSpells.empty()) {
            uint32_t lightFormId = activeLight.value();
            auto* lightBase = GetEquippedLight(player);
            if (!lightBase) return;

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
                uint8_t newType =
                    wantShadows ? static_cast<uint8_t>(LightType::OmniShadow) : static_cast<uint8_t>(LightType::OmniNS);
                SetLightTypeNative(lightBase, newType);

                // Force re-equip to update the reference
                ForceReequipLight(player, wantShadows);

                g_lastShadowStates[lightFormId] = wantShadows;
            }
        }

        // ============ Spell Lights Logic ============
        if (!activeLight.has_value()) {
            for (uint32_t spellFormId : activeSpells) {
                auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellFormId);
                if (!spell || spell->effects.size() == 0) continue;

                // Find the light-associated effect
                for (auto* effect : spell->effects) {
                    if (!effect || !effect->baseEffect) continue;
                    auto* assocForm = effect->baseEffect->data.associatedForm;
                    if (!assocForm) continue;

                    auto* spellLightBase = assocForm->As<RE::TESObjectLIGH>();
                    if (!spellLightBase) continue;

                    uint32_t effectFormId = effect->baseEffect->GetFormID();
                    bool isActive = HasMagicEffect(player, effectFormId);

                    // Skip this spell if it's not currently active on the player, preventing duplicate shadows casting
                    // lights
                    if (!isActive) {
                        continue;
                    }

                    // Only update if state changed from last known state
                    bool lastState = g_lastShadowStates[spellFormId];
                    if (wantShadows != lastState) {
                        DebugPrint("Spell 0x%08X shadow state changed: %s (shadow lights: %d)", spellFormId,
                                   wantShadows ? "ENABLE" : "DISABLE", shadowLightCount);

                        // Get current light type to remember original
                        uint8_t currentLightType = static_cast<uint8_t>(GetLightType(spellLightBase));

                        // Remember the original type
                        if (g_originalLightTypes.find(spellFormId) == g_originalLightTypes.end()) {
                            g_originalLightTypes[spellFormId] = currentLightType;
                        }

                        // Modify the base form
                        uint8_t newType = wantShadows ? static_cast<uint8_t>(LightType::OmniShadow)
                                                      : static_cast<uint8_t>(LightType::OmniNS);
                        SetLightTypeNative(spellLightBase, newType);

                        // Recast the spell
                        player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)
                            ->CastSpellImmediate(spell, false, player, 1.0f, false, 0.0f, nullptr);

                        // Adjust spell light position after recast (only when enabling shadows)
                        if (wantShadows) {
                            std::thread([spellFormId]() {
                                using namespace std::chrono_literals;
                                std::this_thread::sleep_for(200ms);

                                if (auto* tasks = SKSE::GetTaskInterface()) {
                                    tasks->AddTask([spellFormId]() {
                                        auto* pl = RE::PlayerCharacter::GetSingleton();
                                        if (pl) {
                                            AdjustSpellLightPosition(pl, spellFormId);
                                        }
                                    });
                                }
                            }).detach();
                        }

                        // Restore base form after delay
                        std::thread([spellLightBase, spellFormId]() {
                            using namespace std::chrono_literals;
                            std::this_thread::sleep_for(1s);

                            if (auto* tasks = SKSE::GetTaskInterface()) {
                                tasks->AddTask([spellLightBase, spellFormId]() {
                                    uint32_t originalType = g_originalLightTypes[spellFormId];
                                    SetLightTypeNative(spellLightBase, originalType);
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
        // Check if thread is already running
        bool expected = false;
        if (!g_pollThreadRunning.compare_exchange_strong(expected, true)) {
            // Thread is already running
            return;
        }

        std::thread([]() {
            using namespace std::chrono_literals;

            while (g_pollThreadRunning) {
                std::this_thread::sleep_for(std::chrono::seconds(g_config.pollIntervalSeconds));

                // Check again after sleep in case flag was set during sleep
                if (!g_pollThreadRunning) {
                    break;
                }

                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([]() { UpdatePlayerLightShadows(); });
                }
            }
            DebugPrint("Shadow poll thread stopped");
        }).detach();
        DebugPrint("Shadow poll thread started (%ds interval)", g_config.pollIntervalSeconds);
    }

}
