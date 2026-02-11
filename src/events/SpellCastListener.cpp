#include "SpellCastListener.h"

#include "../UpdateLogic.h"
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
        if (!IsPlayerSpellCastEvent(event)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(event->spell);
        if (!spell) {
            return RE::BSEventNotifyControl::kContinue;
        }

        uint32_t spellFormId = spell->GetFormID();

        if (!IsInConfig(spell)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        DebugPrint("SPELLCAST", "Configured spell 0x%08X cast detected. Starting tracking.", spellFormId);

        g_lastShadowStates[spellFormId] = false;

        // EnablePolling();
        // UpdatePlayerLightShadows();

        return RE::BSEventNotifyControl::kContinue;
    }

}
