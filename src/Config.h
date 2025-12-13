#pragma once

#include <string>

namespace TorchShadowLimiter {

    struct Config {
        std::string torchLightNodeName = "AttachLight";
        float torchLightOffsetX = 1.0f;
        float torchLightOffsetY = 5.0f;
        float torchLightOffsetZ = -1.0f;
        int shadowLightLimit = 4;
        bool enableDebug = false;
        int pollIntervalSeconds = 5;
        bool enableInterior = true;
        bool enableExterior = true;
    };

    extern Config g_config;
    void LoadConfig();

}
