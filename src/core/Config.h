#pragma once

#include <string>
#include <vector>

namespace ActorShadowLimiter {
    struct HandHeldLightConfig {
        uint32_t formId = 0;
        std::string plugin;  // Optional: ESP/ESM name for this form ID
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
        std::string plugin;  // Optional: ESP/ESM name for this form ID
        std::string rootNodeName;
        std::string lightNodeName;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float offsetZ = 0.0f;
        float rotateX = 0.0f;
        float rotateY = 0.0f;
        float rotateZ = 0.0f;
    };

    struct EnchantedArmorConfig {
        uint32_t formId = 0;  // Armor's base form ID
        std::string plugin;   // Optional: ESP/ESM name for this form ID
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
        int shadowLightLimitExterior = 3;
        bool enableDebug = false;
        int pollIntervalSeconds = 5;
        bool enableInterior = true;
        bool enableExterior = true;
        bool enableNpc = false;
        bool enableNpcInterior = false;
        bool enableNpcExterior = false;
        float shadowDistanceSafetyMargin = 0.0f;
        float npcMaxDistance = 10000.0f;

        std::vector<HandHeldLightConfig> handHeldLights;
        std::vector<SpellConfig> spells;
        std::vector<EnchantedArmorConfig> enchantedArmors;
    };

    extern Config g_config;
    void LoadConfig();
    void BuildEffectToSpellMapping();
    bool IsInConfig(RE::TESObjectLIGH* lightBase);
    bool IsInConfig(RE::SpellItem* spell);
    bool IsInConfig(RE::TESObjectARMO* armor);
    bool IsInConfig(RE::TESForm* form);
    bool IsValidCell(RE::TESObjectCELL* cell);
    bool IsValidActor(RE::Actor* actor);
    bool IsActorWithinRange(RE::Actor* actor);
    int GetShadowLimit(RE::TESObjectCELL* cell);
}
