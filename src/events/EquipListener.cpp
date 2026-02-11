#include "EquipListener.h"

#include "../LightManager.h"
#include "../UpdateLogic.h"
#include "../actor/ActorTracker.h"
#include "../core/Config.h"
#include "../core/Globals.h"
#include "../utils/Console.h"
#include "../utils/Helpers.h"
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

    /**
     * Torch flow:
     * 1. Actor equips a configured torch
     * 2. Evaluate if shadows can be equipped
     * 3. Equip shadow or static torch
     */
    RE::BSEventNotifyControl EquipListener::ProcessEvent(const RE::TESEquipEvent* event,
                                                         RE::BSTEventSource<RE::TESEquipEvent>*) {
        // Basic validation
        if (!event || !event->actor) {
            return RE::BSEventNotifyControl::kContinue;
        }
        auto* actor = event->actor.get()->As<RE::Actor>();
        if (!IsValidActor(actor)) {
            return RE::BSEventNotifyControl::kContinue;
        }
        auto* form = RE::TESForm::LookupByID(event->baseObject);
        if (!form) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Check if this is a configured light
        if (IsHandheldLight(form)) {
            if (!IsInConfig(form)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Actor equipped a configured light -> Start tracking the actor
            if (event->equipped) {
                ActorTracker::GetSingleton().GetOrCreateActor(actor->GetFormID());
            }
            auto* trackedActor = ActorTracker::GetSingleton().GetActor(actor->GetFormID());
            auto* lightBase = form->As<RE::TESObjectLIGH>();

            // If a re-equip is in action, SHADOW or STATIC - Skip
            if (trackedActor && trackedActor->IsReEquipping()) {
                DebugPrint("EQUIP", actor, "Re-equip in progress for light 0x%08X.", lightBase->GetFormID());
                return RE::BSEventNotifyControl::kContinue;
            }

            // Actor unequipped the light, and not during a re-equip -> Stop tracking the actor
            if (!event->equipped) {
                DebugPrint("EQUIP", actor, "Unequipped light 0x%08X. Stopping tracking.", lightBase->GetFormID());
                ActorTracker::GetSingleton().RemoveActor(actor->GetFormID());
                return RE::BSEventNotifyControl::kContinue;
            }

            // BELOW CODE SHOULD ONLY RUN ON INITIAL EQUIP, NOT RE-EQUIP OR UNEQUIP
            // Safety check - actor should exist since we created it on equip
            if (!trackedActor) {
                DebugPrint("WARN", actor,
                           "Untracked actor detected! Failed to track actor after equipping light 0x%08X.",
                           lightBase->GetFormID());
                return RE::BSEventNotifyControl::kContinue;
            }

            bool isShadowsAllowed = EvaluateActorAndScene(actor);
            ForceReequipLight(actor, lightBase, isShadowsAllowed);

            DebugPrint("EQUIP", actor, "Equipped light 0x%08X with %s shadows.", lightBase->GetFormID(),
                       isShadowsAllowed ? "SHADOW" : "STATIC");

            EnablePolling();
        }
        // else if (IsLightEmittingArmor(form)) {
        //     auto* armorBase = form->As<RE::TESObjectARMO>();
        //     if (!armorBase || !IsInConfig(armorBase)) {
        //         return RE::BSEventNotifyControl::kContinue;
        //     }

        //     DebugPrint("EQUIP", "Configured armor light 0x%08X equipped. Starting tracking.",
        //     armorBase->GetFormID());

        //     EnablePolling();
        //     UpdatePlayerLightShadows(true);
        // }

        return RE::BSEventNotifyControl::kContinue;
    }

}
