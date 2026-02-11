#include "SpellCastListener.h"

#include "../LightManager.h"
#include "../UpdateLogic.h"
#include "../actor/ActorTracker.h"
#include "../core/Config.h"
#include "../core/Globals.h"
#include "../utils/Console.h"
#include "../utils/Helpers.h"

namespace ActorShadowLimiter {

    SpellCastListener* SpellCastListener::GetSingleton() {
        static SpellCastListener instance;
        return &instance;
    }

    void SpellCastListener::Install() {
        BuildEffectToSpellMapping();

        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSource) {
            eventSource->AddEventSink<RE::TESSpellCastEvent>(GetSingleton());
            DebugPrint("INIT", "SpellCastListener installed.");
        }
    }

    RE::BSEventNotifyControl SpellCastListener::ProcessEvent(const RE::TESSpellCastEvent* event,
                                                             RE::BSTEventSource<RE::TESSpellCastEvent>*) {
        // Basic validation
        if (!event || !event->object) {
            return RE::BSEventNotifyControl::kContinue;
        }
        auto* actor = event->object.get()->As<RE::Actor>();
        if (!IsValidActor(actor)) {
            return RE::BSEventNotifyControl::kContinue;
        }
        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(event->spell);
        if (!spell) {
            return RE::BSEventNotifyControl::kContinue;
        }
        if (!IsInConfig(spell)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* trackedActor = ActorTracker::GetSingleton().GetOrCreateActor(actor->GetFormID());

        // Handle case where multiple configured lights are equipped
        if (trackedActor->HasTrackedLight() && trackedActor->GetTrackedLight() != spell->GetFormID()) {
            DebugPrint("WARN", actor, "Already tracking light 0x%08X.", trackedActor->GetTrackedLight().value_or(0));
            return RE::BSEventNotifyControl::kContinue;
        }

        // Safety check - actor should exist since we created it on equip
        if (!trackedActor) {
            DebugPrint("WARN", actor, "Untracked actor detected! Failed to track actor after spell light 0x%08X.",
                       spell->GetFormID());
            return RE::BSEventNotifyControl::kContinue;
        }

        bool isShadowsAllowed = EvaluateActorAndScene(actor);
        if (isShadowsAllowed) {
            ForceCastSpell(actor, spell, true);
        } else {
            trackedActor->SetLightShadowState(spell->GetFormID(), false);
        }

        DebugPrint("SPELL_CAST", "Configured spell 0x%08X cast detected. Starting tracking.", spell->GetFormID());

        EnablePolling();

        return RE::BSEventNotifyControl::kContinue;
    }

}
