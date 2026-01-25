#include "UpdateLogic.h"

#include <chrono>
#include <thread>

#include "Config.h"
#include "Globals.h"
#include "LightManager.h"
#include "SKSE/SKSE.h"
#include "utils/Console.h"
#include "utils/Helpers.h"
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
                                SetLightTypeNative(spellLightBase, static_cast<uint8_t>(LightType::OmniNS));
                                DebugPrint("UPDATE", "Restored spell 0x%08X base form to OmniNS", spellFormId);
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
            uint8_t newType =
                wantShadows ? static_cast<uint8_t>(LightType::OmniShadow) : static_cast<uint8_t>(LightType::OmniNS);
            SetLightTypeNative(lightBase, newType);
            ForceReequipLight(player);
            g_lastShadowStates[lightFormId] = wantShadows;
        }
    }

    void HandleEnchantedArmorLogic(std::vector<uint32_t> activeArmors, bool wantShadows, bool initialEquip) {
        for (uint32_t armorFormId : activeArmors) {
            auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormId);
            if (!armor) continue;

            auto* armorLight = GetLightFromEnchantedArmor(armor);
            if (!armorLight) continue;

            bool lastState = g_lastShadowStates[armorFormId];
            if (wantShadows != lastState) {
                uint8_t newType =
                    wantShadows ? static_cast<uint8_t>(LightType::OmniShadow) : static_cast<uint8_t>(LightType::OmniNS);
                SetLightTypeNative(armorLight, newType);

                // Re-equip armor after a delay to let the base form change settle
                // We can skip this if it's the initial equip, because the light hasnt been spawned yet
                if (!initialEquip) {
                    std::thread([armor, armorLight, armorFormId]() {
                        using namespace std::chrono_literals;
                        std::this_thread::sleep_for(100ms);

                        if (auto* tasks = SKSE::GetTaskInterface()) {
                            tasks->AddTask([armor, armorLight, armorFormId]() {
                                auto* pl = RE::PlayerCharacter::GetSingleton();
                                if (pl) {
                                    ForceReequipArmor(pl, armor);
                                }

                                std::thread([armorLight, armorFormId]() {
                                    std::this_thread::sleep_for(500ms);

                                    if (auto* tasks2 = SKSE::GetTaskInterface()) {
                                        tasks2->AddTask([armorLight, armorFormId]() {
                                            SetLightTypeNative(armorLight, static_cast<uint8_t>(LightType::OmniNS));
                                        });
                                    }
                                }).detach();
                            });
                        }
                    }).detach();
                }

                if (IsOfShadowType(newType)) {
                    std::thread([armorFormId]() {
                        using namespace std::chrono_literals;
                        std::this_thread::sleep_for(500ms);  // These can be quite slow, wait longer

                        if (auto* tasks = SKSE::GetTaskInterface()) {
                            tasks->AddTask([armorFormId]() {
                                auto* pl = RE::PlayerCharacter::GetSingleton();
                                if (pl) {
                                    PrintPlayerNiNodeTree();
                                    AdjustEnchantmentLightPosition(pl, armorFormId);
                                }
                            });
                        }
                    }).detach();
                }

                g_lastShadowStates[armorFormId] = wantShadows;
            }
        }
    }

    void UpdatePlayerLightShadows(bool initialEquip) {
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
        auto activeArmors = GetActiveConfiguredEnchantedArmors(player);
        bool hasActiveTorch = activeLight.has_value();
        bool hasActiveSpells = !activeSpells.empty();
        bool hasActiveArmors = !activeArmors.empty();
        bool noActiveItems = !hasActiveTorch && !hasActiveSpells && !hasActiveArmors;

        if (noActiveItems) {
            if (g_shouldPoll) {
                DebugPrint("UPDATE", "No configured lights, spells, or armors active - disabling polling");
                g_shouldPoll = false;
            }
            return;
        }

        // Check which type currently has shadows enabled
        bool torchHasShadows = hasActiveTorch && g_lastShadowStates[activeLight.value()];
        bool spellHasShadows = hasActiveSpells && std::any_of(activeSpells.begin(), activeSpells.end(),
                                                              [](uint32_t id) { return g_lastShadowStates[id]; });
        bool armorHasShadows = hasActiveArmors && std::any_of(activeArmors.begin(), activeArmors.end(),
                                                              [](uint32_t id) { return g_lastShadowStates[id]; });

        bool anyShadowsEnabled = torchHasShadows || spellHasShadows || armorHasShadows;

        // If shadows are enabled, only handle that type. Otherwise handle all active types.
        if (!anyShadowsEnabled || armorHasShadows) {
            if (hasActiveArmors) {
                HandleEnchantedArmorLogic(activeArmors, wantShadows, initialEquip);
            }
        }
        if (!anyShadowsEnabled || torchHasShadows) {
            if (hasActiveTorch) {
                HandleHeldLightsLogic(activeLight, wantShadows);
            }
        }
        if (!anyShadowsEnabled || spellHasShadows) {
            if (hasActiveSpells) {
                HandleSpellLogic(activeSpells, wantShadows);
            }
        }
    }

    void StartShadowPollThread() {
        // Check if thread is already running
        if (g_pollThreadRunning) {
            return;
        }

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

                // Only poll if we should be polling
                if (g_shouldPoll) {
                    if (auto* tasks = SKSE::GetTaskInterface()) {
                        tasks->AddTask([]() { UpdatePlayerLightShadows(); });
                    }
                }
            }
            DebugPrint("UPDATE", "Shadow poll thread stopped");
        }).detach();
        DebugPrint("UPDATE", "Shadow poll thread started (%ds interval)", g_config.pollIntervalSeconds);
    }

    void EnablePolling() {
        if (!g_shouldPoll) {
            DebugPrint("UPDATE", "Enabling shadow polling");
            g_shouldPoll = true;
            // Start the thread if it's not already running
            StartShadowPollThread();
        }
    }

    void DisablePolling() {
        if (g_shouldPoll) {
            DebugPrint("UPDATE", "Disabling shadow polling");
            g_shouldPoll = false;
        }
    }
}
