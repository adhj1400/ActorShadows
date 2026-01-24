#include "Config.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "utils/Console.h"

namespace ActorShadowLimiter {

    Config g_config;

    bool IsInConfig(RE::TESObjectLIGH* lightBase) {
        for (const auto& config : g_config.handHeldLights) {
            if (config.formId == lightBase->GetFormID()) {
                return true;
            }
        }
        return false;
    }

    bool IsInConfig(RE::SpellItem* spell) {
        for (const auto& config : g_config.spells) {
            if (config.formId == spell->GetFormID()) {
                return true;
            }
        }
        return false;
    }

    bool IsInConfig(RE::TESObjectARMO* armor) {
        for (const auto& config : g_config.enchantedArmors) {
            if (config.formId == armor->GetFormID()) {
                return true;
            }
        }
        return false;
    }

    static std::string ReadFileText(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return "";
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    static std::string ExtractValue(const std::string& json, const std::string& key, size_t startPos) {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = json.find(searchKey, startPos);
        if (keyPos == std::string::npos) return "";

        size_t colonPos = json.find(':', keyPos);
        if (colonPos == std::string::npos) return "";

        size_t valueStart = json.find_first_not_of(" \t\n\r", colonPos + 1);
        if (valueStart == std::string::npos) return "";

        if (json[valueStart] == '"') {
            size_t valueEnd = json.find('"', valueStart + 1);
            if (valueEnd == std::string::npos) return "";
            return json.substr(valueStart + 1, valueEnd - valueStart - 1);
        } else {
            size_t valueEnd = json.find_first_of(",}\n", valueStart);
            if (valueEnd == std::string::npos) valueEnd = json.length();
            std::string value = json.substr(valueStart, valueEnd - valueStart);
            size_t lastNonSpace = value.find_last_not_of(" \t\n\r");
            if (lastNonSpace != std::string::npos) {
                value = value.substr(0, lastNonSpace + 1);
            }
            return value;
        }
    }

    static void LoadJsonConfig() {
        std::string configDir = "Data/SKSE/Plugins/ActorShadows";

        // Check if directory exists
        if (!std::filesystem::exists(configDir)) {
            DebugPrint("CONFIG", "ActorShadows config directory not found: %s", configDir.c_str());
            return;
        }

        int fileCount = 0;

        // Iterate through all .json files in the directory
        for (const auto& entry : std::filesystem::directory_iterator(configDir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".json") continue;

            std::string filePath = entry.path().string();
            std::string json = ReadFileText(filePath);
            if (json.empty()) {
                DebugPrint("CONFIG", "Warning: Could not read file %s", filePath.c_str());
                continue;
            }

            // Extract type field to determine what kind of config this is
            std::string type = ExtractValue(json, "type", 0);
            if (type.empty()) {
                DebugPrint("CONFIG", "Warning: File %s missing 'type' field", filePath.c_str());
                continue;
            }

            if (type == "HandheldLight") {
                HandHeldLightConfig light;
                std::string formIdStr = ExtractValue(json, "formId", 0);
                if (!formIdStr.empty()) {
                    light.formId = std::stoul(formIdStr, nullptr, 0);
                }
                light.plugin = ExtractValue(json, "plugin", 0);
                light.rootNodeName = ExtractValue(json, "rootNodeName", 0);
                light.lightNodeName = ExtractValue(json, "lightNodeName", 0);

                std::string offsetX = ExtractValue(json, "offsetX", 0);
                if (!offsetX.empty()) light.offsetX = std::stof(offsetX);

                std::string offsetY = ExtractValue(json, "offsetY", 0);
                if (!offsetY.empty()) light.offsetY = std::stof(offsetY);

                std::string offsetZ = ExtractValue(json, "offsetZ", 0);
                if (!offsetZ.empty()) light.offsetZ = std::stof(offsetZ);

                std::string rotateX = ExtractValue(json, "rotateX", 0);
                if (!rotateX.empty()) light.rotateX = std::stof(rotateX);

                std::string rotateY = ExtractValue(json, "rotateY", 0);
                if (!rotateY.empty()) light.rotateY = std::stof(rotateY);

                std::string rotateZ = ExtractValue(json, "rotateZ", 0);
                if (!rotateZ.empty()) light.rotateZ = std::stof(rotateZ);

                g_config.handHeldLights.push_back(light);
                DebugPrint("CONFIG", "Loaded HandheldLight from %s", entry.path().filename().string().c_str());

            } else if (type == "SpellLight") {
                SpellConfig spell;
                std::string formIdStr = ExtractValue(json, "formId", 0);
                if (!formIdStr.empty()) {
                    spell.formId = std::stoul(formIdStr, nullptr, 0);
                }
                spell.plugin = ExtractValue(json, "plugin", 0);
                spell.rootNodeName = ExtractValue(json, "rootNodeName", 0);
                spell.lightNodeName = ExtractValue(json, "lightNodeName", 0);

                std::string offsetX = ExtractValue(json, "offsetX", 0);
                if (!offsetX.empty()) spell.offsetX = std::stof(offsetX);

                std::string offsetY = ExtractValue(json, "offsetY", 0);
                if (!offsetY.empty()) spell.offsetY = std::stof(offsetY);

                std::string offsetZ = ExtractValue(json, "offsetZ", 0);
                if (!offsetZ.empty()) spell.offsetZ = std::stof(offsetZ);

                std::string rotateX = ExtractValue(json, "rotateX", 0);
                if (!rotateX.empty()) spell.rotateX = std::stof(rotateX);

                std::string rotateY = ExtractValue(json, "rotateY", 0);
                if (!rotateY.empty()) spell.rotateY = std::stof(rotateY);

                std::string rotateZ = ExtractValue(json, "rotateZ", 0);
                if (!rotateZ.empty()) spell.rotateZ = std::stof(rotateZ);

                g_config.spells.push_back(spell);
                DebugPrint("CONFIG", "Loaded SpellLight from %s", entry.path().filename().string().c_str());

            } else if (type == "EnchantmentLight") {
                EnchantedArmorConfig armor;
                std::string formIdStr = ExtractValue(json, "formId", 0);
                if (!formIdStr.empty()) {
                    armor.formId = std::stoul(formIdStr, nullptr, 0);
                }
                armor.plugin = ExtractValue(json, "plugin", 0);
                armor.rootNodeName = ExtractValue(json, "rootNodeName", 0);
                armor.lightNodeName = ExtractValue(json, "lightNodeName", 0);

                std::string offsetX = ExtractValue(json, "offsetX", 0);
                if (!offsetX.empty()) armor.offsetX = std::stof(offsetX);

                std::string offsetY = ExtractValue(json, "offsetY", 0);
                if (!offsetY.empty()) armor.offsetY = std::stof(offsetY);

                std::string offsetZ = ExtractValue(json, "offsetZ", 0);
                if (!offsetZ.empty()) armor.offsetZ = std::stof(offsetZ);

                std::string rotateX = ExtractValue(json, "rotateX", 0);
                if (!rotateX.empty()) armor.rotateX = std::stof(rotateX);

                std::string rotateY = ExtractValue(json, "rotateY", 0);
                if (!rotateY.empty()) armor.rotateY = std::stof(rotateY);

                std::string rotateZ = ExtractValue(json, "rotateZ", 0);
                if (!rotateZ.empty()) armor.rotateZ = std::stof(rotateZ);

                g_config.enchantedArmors.push_back(armor);
                DebugPrint("CONFIG", "Loaded EnchantmentLight from %s", entry.path().filename().string().c_str());

            } else {
                DebugPrint("CONFIG", "Warning: Unknown type '%s' in file %s", type.c_str(), filePath.c_str());
                continue;
            }

            fileCount++;
        }

        DebugPrint("CONFIG", "Loaded %d light configuration files", fileCount);
        DebugPrint("CONFIG", "Total: %zu hand-held lights, %zu spells, %zu enchanted armors",
                   g_config.handHeldLights.size(), g_config.spells.size(), g_config.enchantedArmors.size());
    }

    void ResolvePluginFormIDs() {
        // Resolve plugin-based form IDs to runtime form IDs for handHeldLights
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            DebugPrint("CONFIG", "Warning: Could not get TESDataHandler for plugin resolution");
            return;
        }

        for (auto& lightConfig : g_config.handHeldLights) {
            if (!lightConfig.plugin.empty() && lightConfig.formId != 0) {
                uint32_t originalLocalFormId = lightConfig.formId;
                auto* light = dataHandler->LookupForm<RE::TESObjectLIGH>(lightConfig.formId, lightConfig.plugin);
                if (light) {
                    lightConfig.formId = light->GetFormID();
                    DebugPrint("CONFIG", "Resolved handHeldLight %s|0x%06X to runtime FormID 0x%08X",
                               lightConfig.plugin.c_str(), originalLocalFormId, lightConfig.formId);
                } else {
                    DebugPrint("CONFIG", "Warning: HandHeldLight %s|0x%06X not found", lightConfig.plugin.c_str(),
                               originalLocalFormId);
                }
            }
        }

        // Resolve plugin-based form IDs for enchantedArmors
        for (auto& armorConfig : g_config.enchantedArmors) {
            if (!armorConfig.plugin.empty() && armorConfig.formId != 0) {
                uint32_t originalLocalFormId = armorConfig.formId;
                auto* armor = dataHandler->LookupForm<RE::TESObjectARMO>(armorConfig.formId, armorConfig.plugin);
                if (armor) {
                    armorConfig.formId = armor->GetFormID();
                    DebugPrint("CONFIG", "Resolved enchantedArmor %s|0x%06X to runtime FormID 0x%08X",
                               armorConfig.plugin.c_str(), originalLocalFormId, armorConfig.formId);
                } else {
                    DebugPrint("CONFIG", "Warning: EnchantedArmor %s|0x%06X not found", armorConfig.plugin.c_str(),
                               originalLocalFormId);
                }
            }
        }
    }

    void LoadConfig() {
        // Load JSON first
        LoadJsonConfig();
        std::string iniPath = "Data/SKSE/Plugins/ActorShadows.ini";
        std::ifstream file(iniPath);

        if (!file.is_open()) {
            // Use defaults if file doesn't exist
            ConsolePrint("ActorShadows.ini not found, using defaults");
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == ';' || line[0] == '#') {
                continue;
            }

            // Find the = separator
            size_t pos = line.find('=');
            if (pos == std::string::npos) {
                continue;
            }

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // Parse settings
            if (key == "ShadowLightLimit") {
                g_config.shadowLightLimit = std::stoi(value);
            } else if (key == "EnableDebug") {
                g_config.enableDebug = (value == "true" || value == "1" || value == "True" || value == "TRUE");
            } else if (key == "PollIntervalSeconds") {
                try {
                    g_config.pollIntervalSeconds = std::stoi(value);
                } catch (...) {
                    // Keep default
                }
            } else if (key == "EnableInterior") {
                g_config.enableInterior = (value == "true" || value == "1" || value == "True" || value == "TRUE");
            } else if (key == "EnableExterior") {
                g_config.enableExterior = (value == "true" || value == "1" || value == "True" || value == "TRUE");
            } else if (key == "MaxSearchRadius") {
                try {
                    g_config.maxSearchRadius = std::stof(value);
                } catch (...) {
                    // Keep default
                }
            } else if (key == "ShadowDistanceSafetyMargin") {
                try {
                    g_config.shadowDistanceSafetyMargin = std::stof(value);
                } catch (...) {
                    // Keep default
                }
            }
        }

        file.close();
        DebugPrint("CONFIG", "ActorShadows config loaded: ShadowLimit=%d, MaxSearchRadius=%.1f, Debug=%s",
                   g_config.shadowLightLimit, g_config.maxSearchRadius, g_config.enableDebug ? "ON" : "OFF");

        // Resolve plugin-based form IDs to runtime form IDs
        ResolvePluginFormIDs();
    }

    void BuildEffectToSpellMapping() {
        // Build mapping from configured spells to their light-emitting effects
        // This allows us to detect which configured spells are active by checking magic effects
        for (auto& spellConfig : g_config.spells) {
            if (spellConfig.formId == 0) continue;

            RE::SpellItem* spell = nullptr;
            uint32_t originalLocalFormId = spellConfig.formId;

            // If plugin is specified, use plugin-based lookup
            if (!spellConfig.plugin.empty()) {
                auto* dataHandler = RE::TESDataHandler::GetSingleton();
                if (dataHandler) {
                    spell = dataHandler->LookupForm<RE::SpellItem>(spellConfig.formId, spellConfig.plugin);
                    if (spell) {
                        // Update formId to runtime ID for faster comparisons later
                        spellConfig.formId = spell->GetFormID();
                        DebugPrint("CONFIG", "Resolved %s|0x%06X to runtime FormID 0x%08X", spellConfig.plugin.c_str(),
                                   originalLocalFormId, spell->GetFormID());
                    }
                }
            } else {
                // Use direct formId lookup
                spell = RE::TESForm::LookupByID<RE::SpellItem>(spellConfig.formId);
            }

            if (!spell || spell->effects.size() == 0) {
                if (!spellConfig.plugin.empty()) {
                    DebugPrint("CONFIG", "Warning: Spell %s|0x%06X not found or has no effects",
                               spellConfig.plugin.c_str(), originalLocalFormId);
                } else {
                    DebugPrint("CONFIG", "Warning: Spell FormID 0x%08X not found or has no effects",
                               spellConfig.formId);
                }
                continue;
            }

            // Find effects with associated lights
            for (auto* effect : spell->effects) {
                if (!effect || !effect->baseEffect) continue;
                auto* assocForm = effect->baseEffect->data.associatedForm;
                if (!assocForm) continue;

                auto* asLight = assocForm->As<RE::TESObjectLIGH>();
                if (asLight) {
                    DebugPrint("CONFIG", "Mapped effect 0x%08X -> spell 0x%08X", effect->baseEffect->GetFormID(),
                               spellConfig.formId);
                }
            }
        }

        DebugPrint("CONFIG", "Built effect-to-spell mapping for %zu configured spells", g_config.spells.size());
    }

}
