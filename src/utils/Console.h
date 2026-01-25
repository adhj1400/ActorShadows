#pragma once

#include <string>

namespace ActorShadowLimiter {
    void InitializeLog();
    void DebugPrint(const std::string& action, const char* format, ...);
    void PrintPlayerNiNodeTree();
    std::string FormatFormIdMessage(uint32_t formId, const std::string& message);
}
