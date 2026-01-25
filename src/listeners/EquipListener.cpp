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

    void HandleHandheldLightEquip(RE::TESObjectLIGH* lightBase) {
        uint32_t lightFormId = lightBase->GetFormID();

        if (HasShadows(lightBase)) {
            g_lastShadowStates[lightFormId] = true;
            return;
        }

        g_lastShadowStates[lightFormId] = false;
        DebugPrint("EQUIP", "Configured hand-held light 0x%08X equipped. Starting tracking.", lightFormId);
    }

    RE::BSEventNotifyControl EquipListener::ProcessEvent(const RE::TESEquipEvent* event,
                                                         RE::BSTEventSource<RE::TESEquipEvent>*) {
        if (!IsPlayerEquipEvent(event) || !event->equipped) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (g_isReequipping) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* form = RE::TESForm::LookupByID(event->baseObject);
        if (!form) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (IsHandheldLight(form)) {
            auto* lightBase = form->As<RE::TESObjectLIGH>();
            if (!lightBase || !IsInConfig(lightBase)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            HandleHandheldLightEquip(lightBase);

            EnablePolling();
            UpdatePlayerLightShadows();
        } else if (IsLightEmittingArmor(form)) {
            auto* armorBase = form->As<RE::TESObjectARMO>();
            if (!armorBase || !IsInConfig(armorBase)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            DebugPrint("EQUIP", "Configured armor light 0x%08X equipped. Starting tracking.", armorBase->GetFormID());

            EnablePolling();
            UpdatePlayerLightShadows(true);
        }

        return RE::BSEventNotifyControl::kContinue;
    }

}
