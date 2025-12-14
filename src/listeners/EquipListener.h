#pragma once
#include "SKSE/SKSE.h"

namespace ActorShadowLimiter {

    class EquipListener : public RE::BSTEventSink<RE::TESEquipEvent> {
    public:
        static EquipListener* GetSingleton();
        static void Install();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event,
                                              RE::BSTEventSource<RE::TESEquipEvent>* source) override;
    };

}
