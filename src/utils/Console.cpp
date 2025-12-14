#include "Console.h"

#include <cstdarg>

#include "../Config.h"

namespace ActorShadowLimiter {

    void ConsolePrint(const char* format, ...) {
        auto* console = RE::ConsoleLog::GetSingleton();
        if (!console) return;

        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        console->Print("[ActorShadows] %s", buffer);
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

        console->Print("[ActorShadows DEBUG] %s", buffer);
    }

}
