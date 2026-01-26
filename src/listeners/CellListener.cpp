#include "CellListener.h"

#include "../Config.h"
#include "../Globals.h"
#include "../LightManager.h"
#include "../UpdateLogic.h"
#include "../utils/ActorLight.h"
#include "../utils/ActorTracker.h"
#include "../utils/Console.h"

namespace ActorShadowLimiter {
    CellListener* CellListener::GetSingleton() {
        static CellListener instance;
        return &instance;
    }

    void CellListener::Install() {
        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSource) {
            eventSource->AddEventSink<RE::TESCellFullyLoadedEvent>(GetSingleton());
            DebugPrint("INIT", "CellListener installed.");
        }
    }

    RE::BSEventNotifyControl CellListener::ProcessEvent(const RE::TESCellFullyLoadedEvent* event,
                                                        RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) {
        if (!event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto activeLight = GetActiveConfiguredLight(player);
        auto activeSpells = GetActiveConfiguredSpells(player);
        auto activeEnchantments = GetActiveConfiguredEnchantedArmors(player);

        if (!activeLight.has_value() && activeSpells.empty() && activeEnchantments.empty()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* cell = player->GetParentCell();
        bool isExterior = cell && cell->IsExteriorCell();

        if (cell && (!wasExterior || !isExterior)) {
            ResetEquippedLightToNoShadow(player);
            ResetActiveSpellsToNoShadow(player);
            ResetActiveEnchantedArmorsToNoShadow(player);
        }

        wasExterior = isExterior;

        // Print NPC node trees for debugging cell transitions
        if (g_config.enableNpcs) {
            PrintNPCNiNodeTrees();
        }

        EnablePolling();

        return RE::BSEventNotifyControl::kContinue;
    }

}
