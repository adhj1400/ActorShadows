#include "Console.h"

#include "../Config.h"

namespace ActorShadowLimiter {
    void DebugPrint(const std::string& action, const char* format, ...) {
        if (!g_config.enableDebug) return;

        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        SKSE::log::debug("[ActorShadows {}] {}", action, buffer);
    }

}
