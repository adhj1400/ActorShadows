#pragma once

#include <string>

namespace ActorShadowLimiter {
    void InitializeLog();
    void DebugPrint(const std::string& action, const char* format, ...);
    void DebugPrint(const std::string& action, RE::Actor* actor, const char* format, ...);
    void PrintNiNodeTree(RE::Actor* actor);
    std::string FormatFormIdMessage(uint32_t formId, const std::string& message);
}
