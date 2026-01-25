#pragma once

#include "RE/Skyrim.h"

namespace ActorShadowLimiter {
    bool IsPlayer(RE::TESObjectREFR* ref);
    bool IsPlayerEquipEvent(const RE::TESEquipEvent* event);
    bool IsPlayerSpellCastEvent(const RE::TESSpellCastEvent* event);
    bool IsHandheldLight(RE::TESForm* form);
    bool IsLightEmittingArmor(RE::TESForm* form);
    void WarnIfLightsHaveShadows();
    void LogUnrestoredLights();
}
