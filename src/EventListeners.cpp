#include "EventListeners.h"

#include "Globals.h"
#include "LightHelpers.h"
#include "SKSE/SKSE.h"
#include "TorchManager.h"
#include "UpdateLogic.h"

namespace TorchShadowLimiter {

    // ============ EquipListener ============

    EquipListener* EquipListener::GetSingleton() {
        static EquipListener instance;
        return &instance;
    }

    void EquipListener::Install() {
        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSource) {
            eventSource->AddEventSink<RE::TESEquipEvent>(GetSingleton());
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("EquipListener installed.");
            }
        }
    }

    RE::BSEventNotifyControl EquipListener::ProcessEvent(const RE::TESEquipEvent* event,
                                                         RE::BSTEventSource<RE::TESEquipEvent>* /*source*/) {
        if (!event || !event->actor) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || event->actor->GetHandle() != player->GetHandle()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (!event->equipped) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* form = RE::TESForm::LookupByID(event->baseObject);
        if (!form || form->GetFormType() != RE::FormType::Light) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* lightBase = form->As<RE::TESObjectLIGH>();
        if (!lightBase) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (auto* console = RE::ConsoleLog::GetSingleton()) {
            console->Print("Torch equipped. Starting polling and adjusting position.");
        }

        // Torch and Candlelight are mutually exclusive
        g_pollCandlelight = false;

        g_pollTorch = true;
        g_lastShadowEnabled = false;
        g_originalTorchLightType = 255;

        // Start polling if not already running
        StartTorchPollThread();

        // Immediate check
        UpdateTorchShadowState_Native();

        // Adjust light position after a short delay to ensure the light node is available
        std::thread([]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(200ms);

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([]() {
                    auto* pl = RE::PlayerCharacter::GetSingleton();
                    if (pl) {
                        AdjustTorchLightPosition(pl);
                    }
                });
            }
        }).detach();

        return RE::BSEventNotifyControl::kContinue;
    }

    // ============ SpellCastListener ============

    SpellCastListener* SpellCastListener::GetSingleton() {
        static SpellCastListener instance;
        return &instance;
    }

    void SpellCastListener::Install() {
        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSource) {
            eventSource->AddEventSink<RE::TESSpellCastEvent>(GetSingleton());
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("SpellCastListener installed.");
            }
        }
    }

    RE::BSEventNotifyControl SpellCastListener::ProcessEvent(const RE::TESSpellCastEvent* event,
                                                             RE::BSTEventSource<RE::TESSpellCastEvent>* /*source*/) {
        if (!event || !event->object) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || event->object->GetHandle() != player->GetHandle()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(event->spell);
        if (!spell) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Check if this is the Candlelight spell
        if (spell->GetFormID() == kCandlelightSpell) {
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("Candlelight cast detected. Starting polling.");
            }

            // Torch and Candlelight are mutually exclusive
            g_pollTorch = false;

            g_pollCandlelight = true;
            g_lastCandlelightShadowEnabled = false;
            g_originalCandlelightLightType = 255;

            // Start polling if not already running
            StartTorchPollThread();

            // Immediate check
            UpdateTorchShadowState_Native();
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    // ============ CellListener ============

    CellListener* CellListener::GetSingleton() {
        static CellListener instance;
        return &instance;
    }

    void CellListener::Install() {
        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSource) {
            eventSource->AddEventSink<RE::TESCellFullyLoadedEvent>(GetSingleton());
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("CellListener installed.");
            }
        }
    }

    RE::BSEventNotifyControl CellListener::ProcessEvent(const RE::TESCellFullyLoadedEvent* event,
                                                        RE::BSTEventSource<RE::TESCellFullyLoadedEvent>* /*source*/) {
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
            std::this_thread::sleep_for(500ms);

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([player]() {
                    // Check if player has a torch equipped
                    bool hasTorch = false;
                    auto* torchBase = GetPlayerTorchBase(player);
                    if (torchBase) {
                        hasTorch = true;
                        if (auto* console = RE::ConsoleLog::GetSingleton()) {
                            console->Print("CellListener: Torch detected (FormID: %08X)", torchBase->GetFormID());
                        }
                    }

                    // Check if Candlelight is active
                    bool hasCandlelight = false;
                    auto* magicTarget = player->GetMagicTarget();
                    if (magicTarget) {
                        auto* activeEffects = magicTarget->GetActiveEffectList();
                        if (activeEffects) {
                            for (auto& effect : *activeEffects) {
                                if (effect && effect->effect && effect->effect->baseEffect) {
                                    if (effect->effect->baseEffect->GetFormID() == kCandlelightEffect) {
                                        hasCandlelight = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    // Only scan if player has torch or Candlelight
                    if (hasTorch || hasCandlelight) {
                        if (auto* console = RE::ConsoleLog::GetSingleton()) {
                            console->Print(
                                "Cell fully loaded. Player has torch/Candlelight - rechecking shadow state.");
                        }

                        // Update polling flags and reset original types
                        if (hasTorch) {
                            g_pollTorch = true;
                            g_originalTorchLightType = 255;
                            g_lastShadowEnabled = false;  // Reset state
                        }
                        if (hasCandlelight) {
                            g_pollCandlelight = true;
                            g_originalCandlelightLightType = 255;
                            g_lastCandlelightShadowEnabled = false;  // Reset state
                        }

                        // Start polling thread
                        StartTorchPollThread();

                        // Immediate check
                        UpdateTorchShadowState_Native();

                        // Force re-equip and position adjustment for torch
                        if (hasTorch) {
                            ForceReequipTorch(player);

                            std::thread([]() {
                                using namespace std::chrono_literals;
                                std::this_thread::sleep_for(200ms);

                                if (auto* tasks = SKSE::GetTaskInterface()) {
                                    tasks->AddTask([]() {
                                        auto* pl = RE::PlayerCharacter::GetSingleton();
                                        if (pl) {
                                            AdjustTorchLightPosition(pl);
                                        }
                                    });
                                }
                            }).detach();
                        }
                    } else {
                        if (auto* console = RE::ConsoleLog::GetSingleton()) {
                            console->Print("Cell fully loaded. Player has no torch/Candlelight - skipping scan.");
                        }
                    }
                });
            }
        }).detach();

        return RE::BSEventNotifyControl::kContinue;
    }

}  // namespace TorchShadowLimiter
