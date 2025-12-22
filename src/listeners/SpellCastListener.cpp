#include "SpellCastListener.h"

#include "../Config.h"
#include "../Globals.h"
#include "../UpdateLogic.h"
#include "../utils/Console.h"
#include "../utils/Helpers.h"

namespace ActorShadowLimiter {

    SpellCastListener* SpellCastListener::GetSingleton() {
        static SpellCastListener instance;
        return &instance;
    }

    void SpellCastListener::Install() {
        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSource) {
            eventSource->AddEventSink<RE::TESSpellCastEvent>(GetSingleton());
            DebugPrint("INIT", "SpellCastListener installed.");
        }
    }

    RE::BSEventNotifyControl SpellCastListener::ProcessEvent(const RE::TESSpellCastEvent* event,
                                                             RE::BSTEventSource<RE::TESSpellCastEvent>*) {
        if (!event || !event->object) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (!IsPlayer(event->object.get())) {
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

        EnablePolling();
        UpdatePlayerLightShadows();

        return RE::BSEventNotifyControl::kContinue;
    }

}
