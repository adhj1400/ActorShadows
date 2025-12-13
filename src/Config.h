#pragma once

#include <string>
#include <vector>

namespace TorchShadowLimiter {

    struct HandHeldLightConfig {
        uint32_t formId = 0;
        std::string nodeName;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float offsetZ = 0.0f;
    };

    struct SpellConfig {
        uint32_t formId = 0;
        std::string nodeName;
    };

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
        float searchRadius = 6000.0f;

        std::vector<HandHeldLightConfig> handHeldLights;
        std::vector<SpellConfig> spells;
    };

    extern Config g_config;
    void LoadConfig();
    void BuildEffectToSpellMapping();

}
