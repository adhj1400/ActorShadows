#pragma once
#include "SKSE/SKSE.h"

namespace ActorShadowLimiter {

    class SpellCastListener : public RE::BSTEventSink<RE::TESSpellCastEvent> {
    public:
        static SpellCastListener* GetSingleton();
        static void Install();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* event,
                                              RE::BSTEventSource<RE::TESSpellCastEvent>* source) override;
    };

}
