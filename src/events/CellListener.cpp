#include "CellListener.h"

#include "../LightManager.h"
#include "../UpdateLogic.h"
#include "../actor/ActorTracker.h"
#include "../core/Config.h"
#include "../core/Globals.h"
#include "../utils/Console.h"

namespace ActorShadowLimiter {
    CellListener* CellListener::GetSingleton() {
        static CellListener instance;
        return &instance;
    }

    void CellListener::Install() {
        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSource) {
            eventSource->AddEventSink<RE::TESCellFullyLoadedEvent>(GetSingleton());
            DebugPrint("INIT", "CellListener installed.");
        }
    }

    RE::BSEventNotifyControl CellListener::ProcessEvent(const RE::TESCellFullyLoadedEvent* event,
                                                        RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) {
        // Sanity checks
        if (!event) {
            return RE::BSEventNotifyControl::kContinue;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // If polling is active - Skip and instead rely on its logic instead
        // If not, we can assume that the game is fresh or no actor has active configured lights
        if (g_pollThreadRunning) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Delay cell load processing by 2 seconds
        std::thread([]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(2000ms);

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([]() {
                    auto* player = RE::PlayerCharacter::GetSingleton();
                    if (!player) {
                        return;
                    }

                    // Check if player have equipped configured lights, newly loaded NPCs often do not
                    auto activeLight = GetActiveConfiguredLight(player);
                    auto activeSpells = GetActiveConfiguredSpells(player);
                    auto activeEnchantments = GetActiveConfiguredEnchantedArmors(player);

                    if (!activeLight.has_value() && activeSpells.empty() && activeEnchantments.empty()) {
                        DebugPrint("CELL_LOAD",
                                   "Cell fully loaded. Player has no active configured light. Skipping tracking.");
                        return;
                    }

                    // Add tracking to the player and rely on polling
                    auto* trackedActor = ActorTracker::GetSingleton().GetOrCreateActor(player->GetFormID());
                    if (!activeEnchantments.empty() && activeEnchantments.size() > 0) {
                        trackedActor->SetTrackedLight(activeEnchantments[0]);
                    } else if (activeLight.has_value()) {
                        trackedActor->SetTrackedLight(activeLight.value());
                    } else if (!activeSpells.empty() && activeSpells.size() > 0) {
                        trackedActor->SetTrackedLight(activeSpells[0]);
                    }

                    EnablePolling(0);

                    DebugPrint("CELL_LOAD",
                               "Cell fully loaded. Player has active configured light(s). Starting tracking.");
                });
            }
        }).detach();

        return RE::BSEventNotifyControl::kContinue;
    }
}
