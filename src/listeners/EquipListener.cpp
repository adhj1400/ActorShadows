#include "EquipListener.h"

#include "../Config.h"
#include "../Globals.h"
#include "../LightManager.h"
#include "../UpdateLogic.h"
#include "../utils/Console.h"
#include "../utils/Light.h"

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

        if (!IsInConfig(lightBase)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (HasShadows(lightBase)) {
            // Restore base form to original type
            if (g_originalLightTypes.find(lightFormId) != g_originalLightTypes.end()) {
                SetLightTypeNative(lightBase, g_originalLightTypes[lightFormId]);
            }

            auto* pl = RE::PlayerCharacter::GetSingleton();
            if (pl) {
                AdjustHeldLightPosition(pl);
            }

            return RE::BSEventNotifyControl::kContinue;
        }

        DebugPrint("EQUIP", "Configured hand-held light 0x%08X equipped. Starting tracking.", lightFormId);

        g_lastShadowStates[lightFormId] = false;
        StartShadowPollThread();
        UpdatePlayerLightShadows();

        return RE::BSEventNotifyControl::kContinue;
    }

}
