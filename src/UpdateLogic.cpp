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
    void HandleSpellLogic(std::vector<std::seed_seq::result_type> activeSpells, bool wantShadows) {
        auto* player = RE::PlayerCharacter::GetSingleton();

        for (uint32_t spellFormId : activeSpells) {
            auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellFormId);
            if (!spell || spell->effects.size() == 0) continue;

            // Find the light-associated effect
            for (auto* effect : spell->effects) {
                if (!effect || !effect->baseEffect) {
                    continue;
                }
                auto* assocForm = effect->baseEffect->data.associatedForm;
                if (!assocForm) {
                    continue;
                }

                auto* spellLightBase = assocForm->As<RE::TESObjectLIGH>();
                if (!spellLightBase) {
                    continue;
                }

                uint32_t effectFormId = effect->baseEffect->GetFormID();
                bool isActive = HasMagicEffect(player, effectFormId);

                if (!isActive) {
                    continue;
                }

                // Only update if state changed from last known state
                bool lastState = g_lastShadowStates[spellFormId];
                if (wantShadows != lastState) {
                    DebugPrint("UPDATE", "Spell 0x%08X shadow state changed: %s", spellFormId,
                               wantShadows ? "ENABLE" : "DISABLE");

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
                        std::this_thread::sleep_for(500ms);

                        if (auto* tasks = SKSE::GetTaskInterface()) {
                            tasks->AddTask([spellLightBase, spellFormId]() {
                                uint32_t originalType = g_originalLightTypes[spellFormId];
                                SetLightTypeNative(spellLightBase, originalType);
                                DebugPrint("UPDATE", "Restored spell 0x%08X base form to original type: %u",
                                           spellFormId, originalType);
                            });
                        }
                    }).detach();

                    g_lastShadowStates[spellFormId] = wantShadows;
                }
            }
        }
    }

    void HandleHeldLightsLogic(std::optional<uint32_t> activeLight, bool wantShadows) {
        uint32_t lightFormId = activeLight.value();
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* lightBase = GetEquippedLight(player);
        bool lastState = g_lastShadowStates[lightFormId];

        if (!lightBase) {
            return;
        }

        if (wantShadows != lastState) {
            uint8_t currentLightType = static_cast<uint8_t>(GetLightType(lightBase));

            if (g_originalLightTypes.find(lightFormId) == g_originalLightTypes.end()) {
                g_originalLightTypes[lightFormId] = currentLightType;
            }

            uint8_t newType =
                wantShadows ? static_cast<uint8_t>(LightType::OmniShadow) : static_cast<uint8_t>(LightType::OmniNS);
            SetLightTypeNative(lightBase, newType);
            ForceReequipLight(player);
            g_lastShadowStates[lightFormId] = wantShadows;
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

        bool isExterior = cell->IsExteriorCell();
        if (isExterior && !g_config.enableExterior) {
            return;
        }
        if (!isExterior && !g_config.enableInterior) {
            return;
        }

        int shadowLightCount = CountNearbyShadowLights();
        bool wantShadows = (shadowLightCount < g_config.shadowLightLimit);
        auto activeSpells = GetActiveConfiguredSpells(player);
        auto activeLight = GetActiveConfiguredLight(player);
        bool hasActiveLight = activeLight.has_value();
        bool hasActiveSpells = !activeSpells.empty();
        bool noActiveItems = !hasActiveLight && !hasActiveSpells;

        if (noActiveItems) {
            g_pollThreadRunning = false;
        } else if (hasActiveLight && !hasActiveSpells) {
            HandleHeldLightsLogic(activeLight, wantShadows);
        } else if (hasActiveSpells && !hasActiveLight) {
            HandleSpellLogic(activeSpells, wantShadows);
        }
    }

    void StartShadowPollThread() {
        bool expected = false;
        if (!g_pollThreadRunning.compare_exchange_strong(expected, true)) {
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
            DebugPrint("UPDATE", "Shadow poll thread stopped");
        }).detach();
        DebugPrint("UPDATE", "Shadow poll thread started (%ds interval)", g_config.pollIntervalSeconds);
    }
}
