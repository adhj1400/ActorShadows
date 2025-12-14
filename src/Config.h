#pragma once

#include <string>
#include <vector>

namespace ActorShadowLimiter {

    struct HandHeldLightConfig {
        uint32_t formId = 0;
        std::string rootNodeName;
        std::string lightNodeName;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float offsetZ = 0.0f;
        float rotateX = 0.0f;  // Pitch
        float rotateY = 0.0f;  // Yaw
        float rotateZ = 0.0f;  // Roll
    };

    struct SpellConfig {
        uint32_t formId = 0;
        std::string rootNodeName;
        std::string lightNodeName;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float offsetZ = 0.0f;
        float rotateX = 0.0f;  // Pitch
        float rotateY = 0.0f;  // Yaw
        float rotateZ = 0.0f;  // Roll
    };

    struct Config {
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
