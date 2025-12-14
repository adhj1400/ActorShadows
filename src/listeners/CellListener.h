#pragma once
#include "SKSE/SKSE.h"

namespace ActorShadowLimiter {

    class CellListener : public RE::BSTEventSink<RE::TESCellFullyLoadedEvent> {
    public:
        static CellListener* GetSingleton();
        static void Install();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* event,
                                              RE::BSTEventSource<RE::TESCellFullyLoadedEvent>* source) override;
    };

}
