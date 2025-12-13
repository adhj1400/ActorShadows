#include "EventListeners.h"

#include "Config.h"
#include "Globals.h"
#include "LightManager.h"
#include "SKSE/SKSE.h"
#include "UpdateLogic.h"
#include "utils/Console.h"
#include "utils/Light.h"
#include "utils/MagicEffect.h"

namespace ActorShadowLimiter {

    // ============ EquipListener ============

    EquipListener* EquipListener::GetSingleton() {
        static EquipListener instance;
        return &instance;
    }

    void EquipListener::Install() {
        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSource) {
            eventSource->AddEventSink<RE::TESEquipEvent>(GetSingleton());
            DebugPrint("EquipListener installed.");
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

        uint32_t lightFormId = lightBase->GetFormID();

        // Check if this light is in our configuration
        bool isConfigured = false;
        for (const auto& config : g_config.handHeldLights) {
            if (config.formId == lightFormId) {
                isConfigured = true;
                break;
            }
        }

        if (!isConfigured) {
            return RE::BSEventNotifyControl::kContinue;
        }

        DebugPrint("Configured hand-held light 0x%08X equipped. Starting tracking.", lightFormId);

        // Add to active lights and clear any spell tracking
        g_activeHandHeldLights.insert(lightFormId);
        g_activeSpells.clear();

        // Reset state for this light
        g_lastShadowStates[lightFormId] = false;

        // Start polling if not already running
        StartShadowPollThread();

        // Immediate check
        UpdatePlayerLightShadows();

        // Adjust light position after a short delay to ensure the light node is available
        std::thread([]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(500ms);

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([]() {
                    auto* pl = RE::PlayerCharacter::GetSingleton();
                    if (pl) {
                        AdjustLightPosition(pl);
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
            DebugPrint("SpellCastListener installed.");
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

        uint32_t spellFormId = spell->GetFormID();

        // Check if this spell is in our configuration
        bool isConfigured = false;
        for (const auto& config : g_config.spells) {
            if (config.formId == spellFormId) {
                isConfigured = true;
                break;
            }
        }

        if (!isConfigured) {
            return RE::BSEventNotifyControl::kContinue;
        }

        DebugPrint("Configured spell 0x%08X cast detected. Starting tracking.", spellFormId);

        // Add to active spells and clear any hand-held light tracking
        g_activeSpells.insert(spellFormId);
        g_activeHandHeldLights.clear();

        // Reset state for this spell
        g_lastShadowStates[spellFormId] = false;

        // Start polling if not already running
        StartShadowPollThread();

        // Immediate check
        UpdatePlayerLightShadows();

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
            DebugPrint("CellListener installed.");
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
            std::this_thread::sleep_for(200ms);

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([player]() {
                    // Check for configured hand-held lights
                    bool hasConfiguredLight = false;
                    auto* lightBase = GetEquippedLight(player);
                    if (lightBase) {
                        uint32_t lightFormId = lightBase->GetFormID();
                        for (const auto& config : g_config.handHeldLights) {
                            if (config.formId == lightFormId) {
                                hasConfiguredLight = true;
                                g_activeHandHeldLights.insert(lightFormId);
                                g_lastShadowStates[lightFormId] = false;
                                DebugPrint("CellListener: Configured light detected (FormID: 0x%08X)", lightFormId);
                                break;
                            }
                        }
                    }

                    // Check for configured active spells
                    bool hasConfiguredSpell = false;
                    auto* magicTarget = player->GetMagicTarget();
                    if (magicTarget) {
                        auto* activeEffects = magicTarget->GetActiveEffectList();
                        if (activeEffects) {
                            for (const auto& spellConfig : g_config.spells) {
                                auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellConfig.formId);
                                if (spell && spell->effects.size() > 0) {
                                    for (auto* effect : spell->effects) {
                                        if (!effect || !effect->baseEffect) continue;
                                        uint32_t effectFormId = effect->baseEffect->GetFormID();

                                        // Check if this effect is active
                                        for (auto& activeEffect : *activeEffects) {
                                            if (activeEffect && activeEffect->effect &&
                                                activeEffect->effect->baseEffect) {
                                                if (activeEffect->effect->baseEffect->GetFormID() == effectFormId) {
                                                    hasConfiguredSpell = true;
                                                    g_activeSpells.insert(spellConfig.formId);
                                                    g_lastShadowStates[spellConfig.formId] = false;
                                                    DebugPrint(
                                                        "CellListener: Configured spell detected (FormID: 0x%08X)",
                                                        spellConfig.formId);
                                                    break;
                                                }
                                            }
                                        }
                                        if (hasConfiguredSpell) break;
                                    }
                                }
                                if (hasConfiguredSpell) break;
                            }
                        }
                    }

                    // Only process if player has configured lights or spells
                    if (hasConfiguredLight || hasConfiguredSpell) {
                        DebugPrint("Cell fully loaded. Player has configured light/spell - rechecking shadow state.");

                        // Start polling thread
                        StartShadowPollThread();

                        // Immediate check
                        UpdatePlayerLightShadows();

                        // Force re-equip and position adjustment for hand-held lights
                        if (hasConfiguredLight) {
                            ForceReequipLight(player);

                            std::thread([]() {
                                using namespace std::chrono_literals;
                                std::this_thread::sleep_for(200ms);

                                if (auto* tasks = SKSE::GetTaskInterface()) {
                                    tasks->AddTask([]() {
                                        auto* pl = RE::PlayerCharacter::GetSingleton();
                                        if (pl) {
                                            AdjustLightPosition(pl);
                                        }
                                    });
                                }
                            }).detach();
                        }
                    } else {
                        DebugPrint("Cell fully loaded. Player has no configured lights/spells - skipping scan.");
                    }
                });
            }
        }).detach();

        return RE::BSEventNotifyControl::kContinue;
    }

}
