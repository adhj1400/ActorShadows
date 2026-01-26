#include "EquipListener.h"

#include "../Config.h"
#include "../Globals.h"
#include "../LightManager.h"
#include "../UpdateLogic.h"
#include "../utils/ActorLight.h"
#include "../utils/ActorTracker.h"
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

    void SetInitialValues(RE::Actor* actor, RE::TESObjectLIGH* lightBase) {
        uint32_t lightFormId = lightBase->GetFormID();
        uint32_t actorFormId = actor->GetFormID();
        bool isPlayer = actor->IsPlayerRef();

        // Check if already tracked
        auto& tracker = ActorTracker::GetSingleton();
        bool isNewLight = !tracker.HasShadowState(actorFormId, lightFormId);

        // Set initial state based on current shadows
        bool currentlyHasShadows = HasShadows(lightBase);
        tracker.SetShadowState(actorFormId, lightFormId, currentlyHasShadows);

        // Only log for new lights, not re-equips
        if (isNewLight) {
            if (isPlayer) {
                DebugPrint("EQUIP", "Player equipped configured hand-held light 0x%08X. Starting tracking.",
                           lightFormId);
            } else {
                const char* actorName = actor->GetName();
                if (!actorName) actorName = "<unnamed>";
                DebugPrint("EQUIP", "NPC '%s' (0x%08X) equipped configured light 0x%08X. Starting tracking.", actorName,
                           actorFormId, lightFormId);
            }
        }
    }

    RE::Actor* GetActorFromEvent(const RE::TESEquipEvent* event, bool isPlayer) {
        if (!event) return nullptr;

        uint32_t actorFormId = event->actor->GetFormID();

        if (isPlayer) {
            return RE::PlayerCharacter::GetSingleton();
        } else {
            auto* form = RE::TESForm::LookupByID(actorFormId);
            return form ? form->As<RE::Actor>() : nullptr;
        }
    }

    RE::BSEventNotifyControl EquipListener::ProcessEvent(const RE::TESEquipEvent* event,
                                                         RE::BSTEventSource<RE::TESEquipEvent>*) {
        if (!event || !event->equipped) {
            return RE::BSEventNotifyControl::kContinue;
        }

        bool isPlayer = IsPlayerEquipEvent(event);

        // If not player, check if NPC support is enabled
        if (!isPlayer && !g_config.enableNpcs) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Get actor FormID to check re-equip state
        uint32_t actorFormId = event->actor->GetFormID();
        auto& tracker = ActorTracker::GetSingleton();
        if (tracker.IsReequipping(actorFormId)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Get actor (for both player and NPCs)
        RE::Actor* actor = GetActorFromEvent(event, isPlayer);
        if (!actor) {
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

            // Track actor (both player and NPCs)
            tracker.AddActor(actor->GetFormID());

            SetInitialValues(actor, lightBase);

            EnablePolling();
            if (isPlayer) {
                UpdateLightShadows();
            }
        } else if (IsLightEmittingArmor(form)) {
            auto* armorBase = form->As<RE::TESObjectARMO>();
            if (!armorBase || !IsInConfig(armorBase)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Track actor (both player and NPCs)
            tracker.AddActor(actor->GetFormID());

            DebugPrint("EQUIP", "Configured armor light 0x%08X equipped. Starting tracking.", armorBase->GetFormID());

            EnablePolling();
            UpdateLightShadows(true);
        }

        return RE::BSEventNotifyControl::kContinue;
    }

}
