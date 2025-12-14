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
        float rotateX = 0.0f;
        float rotateY = 0.0f;
        float rotateZ = 0.0f;
    };

    struct SpellConfig {
        uint32_t formId = 0;
        std::string rootNodeName;
        std::string lightNodeName;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float offsetZ = 0.0f;
        float rotateX = 0.0f;
        float rotateY = 0.0f;
        float rotateZ = 0.0f;
    };

    struct Config {
        int shadowLightLimit = 4;
        bool enableDebug = false;
        int pollIntervalSeconds = 5;
        bool enableInterior = true;
        bool enableExterior = true;
        float maxSearchRadius = 6000.0f;

        std::vector<HandHeldLightConfig> handHeldLights;
        std::vector<SpellConfig> spells;
    };

    extern Config g_config;
    void LoadConfig();
    void BuildEffectToSpellMapping();

}
