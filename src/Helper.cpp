#include "Helper.h"

#include <cstdarg>

#include "Config.h"

namespace TorchShadowLimiter {

    void ConsolePrint(const char* format, ...) {
        auto* console = RE::ConsoleLog::GetSingleton();
        if (!console) return;

        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        console->Print("%s", buffer);
    }

    void DebugPrint(const char* format, ...) {
        if (!g_config.enableDebug) return;

        auto* console = RE::ConsoleLog::GetSingleton();
        if (!console) return;

        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        console->Print("[DEBUG] %s", buffer);
    }

    bool HasMagicEffect(RE::Actor* actor, RE::FormID effectFormID) {
        if (!actor) {
            return false;
        }

        auto* effect = RE::TESForm::LookupByID<RE::EffectSetting>(effectFormID);
        if (!effect) {
            return false;
        }

        auto* magicTarget = actor->GetMagicTarget();
        if (!magicTarget) {
            return false;
        }

        return magicTarget->HasMagicEffect(effect);
    }

}
