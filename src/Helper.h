#pragma once

namespace TorchShadowLimiter {

    void ConsolePrint(const char* format, ...);
    void DebugPrint(const char* format, ...);
    bool HasMagicEffect(RE::Actor* actor, RE::FormID effectFormID);
}
