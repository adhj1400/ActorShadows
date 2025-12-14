#include "SpellCastListener.h"

#include "../Config.h"
#include "../Globals.h"
#include "../UpdateLogic.h"
#include "../utils/Console.h"

namespace ActorShadowLimiter {

    SpellCastListener* SpellCastListener::GetSingleton() {
        static SpellCastListener instance;
        return &instance;
    }

    void SpellCastListener::Install() {
        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSource) {
            eventSource->AddEventSink<RE::TESSpellCastEvent>(GetSingleton());
            DebugPrint("SpellCastListener installed.");
        }
    }

    RE::BSEventNotifyControl SpellCastListener::ProcessEvent(const RE::TESSpellCastEvent* event,
                                                             RE::BSTEventSource<RE::TESSpellCastEvent>*) {
        if (!event || !event->object) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || event->object->GetHandle() != player->GetHandle()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(event->spell);
        if (!spell) {
            return RE::BSEventNotifyControl::kContinue;
        }

        uint32_t spellFormId = spell->GetFormID();

        // Check if this spell is in our configuration
        bool isConfigured = false;
        for (const auto& config : g_config.spells) {
            if (config.formId == spellFormId) {
                isConfigured = true;
                break;
            }
        }

        if (!isConfigured) {
            return RE::BSEventNotifyControl::kContinue;
        }

        DebugPrint("Configured spell 0x%08X cast detected. Starting tracking.", spellFormId);

        // Reset state for this spell
        g_lastShadowStates[spellFormId] = false;

        // Start polling if not already running
        StartShadowPollThread();

        // Immediate check
        UpdatePlayerLightShadows();

        return RE::BSEventNotifyControl::kContinue;
    }

}
