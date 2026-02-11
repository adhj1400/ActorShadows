#include "UpdateLogic.h"

#include <chrono>
#include <thread>

#include "LightManager.h"
#include "SKSE/SKSE.h"
#include "actor/ActorTracker.h"
#include "actor/TrackedActor.h"
#include "core/Config.h"
#include "core/Globals.h"
#include "utils/Cleanup.h"
#include "utils/Console.h"
#include "utils/Helpers.h"
#include "utils/Light.h"
#include "utils/MagicEffect.h"

namespace ActorShadowLimiter {
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

        // First pass: Always enforce distance limits on all tracked actors
        auto allTrackedActorIds = ActorTracker::GetSingleton().GetAllTrackedActorIds();
        for (uint32_t actorFormId : allTrackedActorIds) {
            auto* trackedActor = ActorTracker::GetSingleton().GetActor(actorFormId);
            if (!trackedActor || !trackedActor->HasTrackedLight()) {
                ActorTracker::GetSingleton().RemoveActor(actorFormId);
                continue;
            }

            auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormId);
            if (!actor || !IsValidActor(actor)) {
                ActorTracker::GetSingleton().RemoveActor(actorFormId);
                continue;
            }

            // Enforce distance limit: disable shadows for actors beyond max range
            if (!IsActorWithinRange(actor)) {
                auto trackedLight = trackedActor->GetTrackedLight();
                if (trackedLight.has_value()) {
                    uint32_t lightTypeFormId = trackedLight.value();
                    if (trackedActor->GetLightShadowState(lightTypeFormId)) {
                        auto* form = RE::TESForm::LookupByID<RE::TESForm>(lightTypeFormId);
                        if (form) {
                            DebugPrint("SCAN", actor, "Actor beyond max range, disabling shadows");
                            if (IsHandheldLight(form)) {
                                ForceReEquipLight(actor, form->As<RE::TESObjectLIGH>(), false);
                            }
                            if (IsLightEmittingArmor(form)) {
                                ForceReEquipArmor(actor, form->As<RE::TESObjectARMO>(), false);
                            }
                            if (IsSpellLight(form)) {
                                ForceCastSpell(actor, form->As<RE::SpellItem>(), false);
                            }
                        }
                    }
                }
            }
        }

        // Count nearby shadow-casting lights and determine if we want shadows enabled
        int shadowLightCount = CountNearbyShadowLights();
        int shadowLimit = GetShadowLimit(cell);
        bool shadowsAllowed = (shadowLightCount < shadowLimit);

        // Second pass: Adjust NPC lights based on shadow limit
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

                auto* trackedActor = ActorTracker::GetSingleton().GetActor(actorFormId);
                if (!trackedActor || !trackedActor->HasTrackedLight()) {
                    continue;
                }

                auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormId);
                if (!actor || !IsValidActor(actor)) {
                    continue;
                }

                // Skip actors that are out of range (already handled in first pass)
                if (!IsActorWithinRange(actor)) {
                    continue;
                }

                auto trackedLight = trackedActor->GetTrackedLight();
                if (!trackedLight.has_value()) {
                    continue;
                }

                uint32_t lightFormId = trackedLight.value();
                auto* form = RE::TESForm::LookupByID<RE::TESForm>(lightFormId);
                if (!form) {
                    continue;
                }

                // Regular shadow limit processing
                bool isNewState = shadowsAllowed != trackedActor->GetLightShadowState(lightFormId);
                if (isNewState) {
                    DebugPrint("SCAN", actor, "State change required, changing light to: %s",
                               shadowsAllowed ? "SHADOWS" : "STATIC");
                    if (IsHandheldLight(form)) {
                        ForceReEquipLight(actor, form->As<RE::TESObjectLIGH>(), shadowsAllowed);
                    }
                    if (IsLightEmittingArmor(form)) {
                        ForceReEquipArmor(actor, form->As<RE::TESObjectARMO>(), shadowsAllowed);
                    }
                    if (IsSpellLight(form)) {
                        ForceCastSpell(actor, form->As<RE::SpellItem>(), shadowsAllowed);
                    }
                    processed++;
                }
            }
        }

        // Start duplicate removal thread if any actors have shadows enabled
        // The thread will auto-stop when no actors have shadows
        if (g_config.enableDuplicateFix && shadowsAllowed) {
            StartDuplicateRemovalThread();
        }
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
    void EnablePolling(int delayInSeconds) {
        // Delay polling start by 4 seconds on a background thread, then execute on main thread
        std::thread([=]() {
            std::this_thread::sleep_for(std::chrono::seconds(delayInSeconds));

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

    void StopShadowPollThread() {
        if (g_shouldPoll) {
            DebugPrint("UPDATE", "Disabling shadow polling");
            g_shouldPoll = false;
        }
    }
}
