#include "EquipListener.h"

#include "../Config.h"
#include "../Globals.h"
#include "../LightManager.h"
#include "../UpdateLogic.h"
#include "../utils/Console.h"
#include "../utils/Helpers.h"
#include "../utils/Light.h"
#include "../utils/ShadowCounter.h"

namespace ActorShadowLimiter {

    EquipListener* EquipListener::GetSingleton() {
        static EquipListener instance;
        return &instance;
    }

    void EquipListener::Install() {
        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSource) {
            eventSource->AddEventSink<RE::TESEquipEvent>(GetSingleton());
            DebugPrint("INIT", "EquipListener installed.");
        }
    }

    RE::BSEventNotifyControl EquipListener::ProcessEvent(const RE::TESEquipEvent* event,
                                                         RE::BSTEventSource<RE::TESEquipEvent>*) {
        if (!event || !event->actor) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* actor = event->actor.get();
        if (!IsPlayer(actor)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        DebugPrint("EQUIP_DEBUG", "TESEquipEvent fired - equipped: %d, baseObject: 0x%08X", event->equipped,
                   event->baseObject);

        if (!event->equipped) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* form = RE::TESForm::LookupByID(event->baseObject);
        if (!form) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Handle hand-held lights
        if (form->GetFormType() == RE::FormType::Light) {
            auto* lightBase = form->As<RE::TESObjectLIGH>();
            if (!lightBase || !IsInConfig(lightBase)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            uint32_t lightFormId = lightBase->GetFormID();

            if (HasShadows(lightBase)) {
                // Restore base form to original type
                if (g_originalLightTypes.find(lightFormId) != g_originalLightTypes.end()) {
                    SetLightTypeNative(lightBase, g_originalLightTypes[lightFormId]);
                }

                auto* player = RE::PlayerCharacter::GetSingleton();

                if (player) {
                    AdjustHeldLightPosition(player);
                }
                g_lastShadowStates[lightFormId] = true;

                return RE::BSEventNotifyControl::kContinue;
            } else {
                g_lastShadowStates[lightFormId] = false;
                UpdatePlayerLightShadows();
            }

            DebugPrint("EQUIP", "Configured hand-held light 0x%08X equipped. Starting tracking.", lightFormId);

            EnablePolling();

            return RE::BSEventNotifyControl::kContinue;
        }

        // Handle enchanted armors
        if (form->GetFormType() == RE::FormType::Armor) {
            auto* armorBase = form->As<RE::TESObjectARMO>();
            if (!armorBase || !IsInConfig(armorBase)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            EnablePolling();
            UpdatePlayerLightShadows(true);
        }

        return RE::BSEventNotifyControl::kContinue;
    }

}
