#include "UpdateLogic.h"

#include <chrono>
#include <thread>

#include "LightManager.h"
#include "SKSE/SKSE.h"
#include "actor/ActorTracker.h"
#include "actor/TrackedActor.h"
#include "core/Config.h"
#include "core/Globals.h"
#include "utils/Console.h"
#include "utils/Helpers.h"
#include "utils/Light.h"
#include "utils/MagicEffect.h"

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

    /**
     * Evaluate the scene and returns wether shadows can be applied or not.
     */
    bool EvaluateActorAndScene(RE::Actor* actor) {
        auto* origoActor = RE::PlayerCharacter::GetSingleton();
        if (!origoActor) {
            return false;
        }

        auto* cell = origoActor->GetParentCell();
        if (!IsValidCell(cell)) {
            return false;
        }

        // Fetch the actor
        auto* trackedActor = ActorTracker::GetSingleton().GetActor(actor->GetFormID());

        // If the actor somehow have been removed from the tracker - skip
        if (!trackedActor) {
            DebugPrint("WARN", actor, "Non-tracked actor was sent for evaluation. Skipping.");
            return false;
        }

        // Scan the scene - are we allowed to have shadows active?
        int shadowLightCount = CountNearbyShadowLights();
        int shadowLimit = GetShadowLimit(cell);

        return shadowLightCount < shadowLimit;
    }

    /**
     * Main, non-actor-specific evaluation loop logic that runs continuously.
     * Evaluates the scene and each of the tracked actors, force re-equips where necessary.
     */
    void UpdateTrackedLights() {
        // Sanity checks
        // Note: Cleanup should be done in the CellListener, i.e. if the player moves from a valid to a non-valid cell.
        auto* origoActor = RE::PlayerCharacter::GetSingleton();
        if (!origoActor) {
            return;
        }
        auto* cell = origoActor->GetParentCell();
        if (!IsValidCell(cell)) {
            return;
        }

        // Count nearby shadow-casting lights and determine if we want shadows enabled
        int shadowLightCount = CountNearbyShadowLights();
        int shadowLimit = GetShadowLimit(cell);
        bool shadowsAllowed = (shadowLightCount < shadowLimit);

        // Adjust NPC lights based on shadow limit
        int actorCountToProcess = std::abs(shadowLightCount - shadowLimit);
        if (actorCountToProcess > 0) {
            // Sort actors by distance: closest first when enabling shadows, furthest first when disabling
            auto trackedActorIds = ActorTracker::GetSingleton().GetAllTrackedActorIds(true, shadowsAllowed);
            int processed = 0;

            // Check all tracked actors and re-equip if needed
            for (uint32_t actorFormId : trackedActorIds) {
                if (processed >= actorCountToProcess) {
                    break;
                }

                // Cleanup - Trailing actors
                auto* trackedActor = ActorTracker::GetSingleton().GetActor(actorFormId);
                if (!trackedActor || !trackedActor->HasTrackedLight()) {
                    ActorTracker::GetSingleton().RemoveActor(actorFormId);
                    continue;
                }

                // Cleanup - Invalid actors
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormId);
                if (!actor || !IsValidActor(actor)) {
                    ActorTracker::GetSingleton().RemoveActor(actorFormId);
                    continue;
                }

                // Get the tracked light for this actor
                auto trackedLight = trackedActor->GetTrackedLight();
                if (!trackedLight.has_value()) {
                    continue;
                }

                uint32_t lightFormId = trackedLight.value();
                auto* lightBase = RE::TESForm::LookupByID<RE::TESObjectLIGH>(lightFormId);
                if (!lightBase) {
                    continue;
                }

                bool isNewState = shadowsAllowed != trackedActor->GetLightShadowState(lightFormId);
                if (isNewState) {
                    DebugPrint("SCAN", actor, "State change required, changing light to: %s",
                               shadowsAllowed ? "SHADOWS" : "STATIC");
                    ForceReequipLight(actor, lightBase, shadowsAllowed);
                }
                processed++;
            }
        }

        // auto activeSpells = GetActiveConfiguredSpells(player);
        // auto activeLight = GetActiveConfiguredLight(player);
        // auto activeArmors = GetActiveConfiguredEnchantedArmors(player);
        // bool hasActiveTorch = activeLight.has_value();
        // bool hasActiveSpells = !activeSpells.empty();
        // bool hasActiveArmors = !activeArmors.empty();
        // bool noActiveItems = !hasActiveTorch && !hasActiveSpells && !hasActiveArmors;

        // if (noActiveItems) {
        //     if (g_shouldPoll) {
        //         DebugPrint("UPDATE", "No configured lights, spells, or armors active - disabling polling");
        //         g_shouldPoll = false;
        //     }
        //     return;
        // }

        // // Check which type currently has shadows enabled
        // bool torchHasShadows = hasActiveTorch && g_lastShadowStates[activeLight.value()];
        // bool spellHasShadows = hasActiveSpells && std::any_of(activeSpells.begin(), activeSpells.end(),
        //                                                       [](uint32_t id) { return g_lastShadowStates[id]; });
        // bool armorHasShadows = hasActiveArmors && std::any_of(activeArmors.begin(), activeArmors.end(),
        //                                                       [](uint32_t id) { return g_lastShadowStates[id]; });

        // bool anyShadowsEnabled = torchHasShadows || spellHasShadows || armorHasShadows;

        // // If shadows are enabled, only handle that type. Otherwise handle all active types.
        // if (!anyShadowsEnabled || armorHasShadows) {
        //     if (hasActiveArmors) {
        //         HandleEnchantedArmorLogic(activeArmors, wantShadows, false);  // TODO: fix the last parameter
        //     }
        // }
        // if (!anyShadowsEnabled || torchHasShadows) {
        //     if (hasActiveTorch) {
        //         HandleHeldLightsLogic(activeLight, wantShadows);
        //     }
        // }
        // if (!anyShadowsEnabled || spellHasShadows) {
        //     if (hasActiveSpells) {
        //         HandleSpellLogic(activeSpells, wantShadows);
        //     }
        // }
    }

    void StartShadowPollThread() {
        // Atomic, thread-safe check if thread is already running
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
                        tasks->AddTask([]() { UpdateTrackedLights(); });
                    }
                }
            }
            DebugPrint("UPDATE", "Shadow poll thread stopped");
        }).detach();
        DebugPrint("UPDATE", "Shadow poll thread started (%ds interval)", g_config.pollIntervalSeconds);
    }

    /**
     * Tries to enable polling, if not already enabled.
     */
    void EnablePolling() {
        // Delay polling start by 4 seconds on a background thread, then execute on main thread
        std::thread([=]() {
            std::this_thread::sleep_for(std::chrono::seconds(4));

            auto* taskQueue = SKSE::GetTaskInterface();
            if (taskQueue) {
                taskQueue->AddTask([]() {
                    // Start polling if it's not already running
                    if (!g_shouldPoll) {
                        DebugPrint("UPDATE", "Enabling shadow polling");
                        g_shouldPoll = true;
                        StartShadowPollThread();
                    }
                });
            }
        }).detach();
    }

    void DisablePolling() {
        if (g_shouldPoll) {
            DebugPrint("UPDATE", "Disabling shadow polling");
            g_shouldPoll = false;
        }
    }
}
