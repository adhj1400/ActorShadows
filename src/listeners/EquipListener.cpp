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

        // Check if the light already has shadows - if so, let the poller handle it
        uint32_t currentType = GetLightType(lightBase);
        if (currentType == 1 || currentType == 3) {  // OmniShadow or SpotShadow
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

        // Reset state for this light (it's a no-shadow variant)
        g_lastShadowStates[lightFormId] = false;

        // Start polling if not already running
        StartShadowPollThread();

        // Immediate check
        UpdatePlayerLightShadows();

        return RE::BSEventNotifyControl::kContinue;
    }

}
