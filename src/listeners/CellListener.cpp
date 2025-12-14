#include "CellListener.h"

#include "../Config.h"
#include "../Globals.h"
#include "../LightManager.h"
#include "../UpdateLogic.h"
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
            DebugPrint("CellListener installed.");
        }
    }

    RE::BSEventNotifyControl CellListener::ProcessEvent(const RE::TESCellFullyLoadedEvent* event,
                                                        RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) {
        if (!event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Delay check to allow equipment to fully load
        std::thread([player]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(200ms);

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([player]() {
                    DebugPrint("Cell fully loaded. Rechecking shadow state.");

                    // Start polling thread
                    StartShadowPollThread();

                    // Immediate check (will scan for active spells automatically)
                    UpdatePlayerLightShadows();
                });
            }
        }).detach();

        return RE::BSEventNotifyControl::kContinue;
    }

}
