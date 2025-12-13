#pragma once
#include "SKSE/SKSE.h"

namespace TorchShadowLimiter {

    class EquipListener : public RE::BSTEventSink<RE::TESEquipEvent> {
    public:
        static EquipListener* GetSingleton();
        static void Install();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event,
                                              RE::BSTEventSource<RE::TESEquipEvent>* source) override;
    };

    class SpellCastListener : public RE::BSTEventSink<RE::TESSpellCastEvent> {
    public:
        static SpellCastListener* GetSingleton();
        static void Install();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* event,
                                              RE::BSTEventSource<RE::TESSpellCastEvent>* source) override;
    };

    class CellListener : public RE::BSTEventSink<RE::TESCellFullyLoadedEvent> {
    public:
        static CellListener* GetSingleton();
        static void Install();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* event,
                                              RE::BSTEventSource<RE::TESCellFullyLoadedEvent>* source) override;
    };

}
