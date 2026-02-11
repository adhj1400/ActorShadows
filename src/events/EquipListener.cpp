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
        if (!IsInConfig(form)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Track actor if equipping
        if (event->equipped) {
            ActorTracker::GetSingleton().GetOrCreateActor(actor->GetFormID());
        }

        // If a re-equip is in action, shadow or static - Skip execution
        // We are assuming that actor is being tracked, otherwise skip execution
        auto* trackedActor = ActorTracker::GetSingleton().GetActor(actor->GetFormID());
        if (trackedActor && trackedActor->IsReEquipping()) {
            DebugPrint("EQUIP", actor, "Re-equip in progress for light 0x%08X.", form->GetFormID());
            return RE::BSEventNotifyControl::kContinue;
        }

        // Actor unequipped the light, and not during a re-equip -> Stop tracking the actor
        if (!event->equipped) {
            DebugPrint("EQUIP", actor, "Unequipped light 0x%08X. Stopping tracking.", form->GetFormID());
            ActorTracker::GetSingleton().RemoveActor(actor->GetFormID());
            return RE::BSEventNotifyControl::kContinue;
        }

        // Handle case where multiple configured lights are equipped
        if (trackedActor->HasTrackedLight() && trackedActor->GetTrackedLight() != form->GetFormID()) {
            DebugPrint("WARN", actor, "Already tracking light 0x%08X.", trackedActor->GetTrackedLight().value_or(0));
            return RE::BSEventNotifyControl::kContinue;
        }

        // Initial equip logic below, should never run if re-equip or unequip
        // Safety check - actor should exist since we created it on equip
        if (!trackedActor) {
            DebugPrint("WARN", actor, "Untracked actor detected! Failed to track actor after equipping light 0x%08X.",
                       form->GetFormID());
            return RE::BSEventNotifyControl::kContinue;
        }

        // Handle different kinds of equipped lights
        bool isShadowsAllowed = EvaluateActorAndScene(actor);
        if (IsHandheldLight(form)) {
            ForceReEquipLight(actor, form->As<RE::TESObjectLIGH>(), isShadowsAllowed);
        }
        if (IsLightEmittingArmor(form)) {
            ForceReEquipArmor(actor, form->As<RE::TESObjectARMO>(), isShadowsAllowed);
        }
        DebugPrint("EQUIP", actor, "Equipped light 0x%08X with %s light variant.", form->GetFormID(),
                   isShadowsAllowed ? "SHADOW" : "STATIC");

        EnablePolling();

        return RE::BSEventNotifyControl::kContinue;
    }
}
