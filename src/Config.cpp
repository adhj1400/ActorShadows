#include "Config.h"

#include <fstream>
#include <sstream>

#include "utils/Console.h"

namespace ActorShadowLimiter {

    Config g_config;

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
        std::string jsonPath = "Data/SKSE/Plugins/ActorShadows.json";
        std::string json = ReadFileText(jsonPath);
        if (json.empty()) return;

        // Parse handHeldLights array
        size_t hlStart = json.find("\"handHeldLights\"");
        if (hlStart != std::string::npos) {
            size_t arrayStart = json.find('[', hlStart);
            size_t arrayEnd = json.find(']', arrayStart);
            if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                size_t objStart = arrayStart;
                while ((objStart = json.find('{', objStart)) != std::string::npos && objStart < arrayEnd) {
                    size_t objEnd = json.find('}', objStart);
                    if (objEnd == std::string::npos || objEnd > arrayEnd) break;

                    HandHeldLightConfig light;
                    std::string formIdStr = ExtractValue(json, "formId", objStart);
                    if (!formIdStr.empty()) {
                        light.formId = std::stoul(formIdStr, nullptr, 0);
                    }
                    light.nodeName = ExtractValue(json, "nodeName", objStart);

                    std::string offsetX = ExtractValue(json, "offsetX", objStart);
                    if (!offsetX.empty()) light.offsetX = std::stof(offsetX);

                    std::string offsetY = ExtractValue(json, "offsetY", objStart);
                    if (!offsetY.empty()) light.offsetY = std::stof(offsetY);

                    std::string offsetZ = ExtractValue(json, "offsetZ", objStart);
                    if (!offsetZ.empty()) light.offsetZ = std::stof(offsetZ);

                    g_config.handHeldLights.push_back(light);
                    objStart = objEnd + 1;
                }
            }
        }

        // Parse spells array
        size_t spellStart = json.find("\"spells\"");
        if (spellStart != std::string::npos) {
            size_t arrayStart = json.find('[', spellStart);
            size_t arrayEnd = json.find(']', arrayStart);
            if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                size_t objStart = arrayStart;
                while ((objStart = json.find('{', objStart)) != std::string::npos && objStart < arrayEnd) {
                    size_t objEnd = json.find('}', objStart);
                    if (objEnd == std::string::npos || objEnd > arrayEnd) break;

                    SpellConfig spell;
                    std::string formIdStr = ExtractValue(json, "formId", objStart);
                    if (!formIdStr.empty()) {
                        spell.formId = std::stoul(formIdStr, nullptr, 0);
                    }
                    spell.nodeName = ExtractValue(json, "nodeName", objStart);

                    g_config.spells.push_back(spell);
                    objStart = objEnd + 1;
                }
            }
        }

        DebugPrint("Loaded %zu hand-held lights and %zu spells from ActorShadows.json", g_config.handHeldLights.size(),
                   g_config.spells.size());
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
            } else if (key == "SearchRadius") {
                try {
                    g_config.searchRadius = std::stof(value);
                } catch (...) {
                    // Keep default
                }
            }
        }

        file.close();
        DebugPrint("ActorShadows config loaded: ShadowLimit=%d, SearchRadius=%.1f, Debug=%s", g_config.shadowLightLimit,
                   g_config.searchRadius, g_config.enableDebug ? "ON" : "OFF");
    }

    void BuildEffectToSpellMapping() {
        // Build mapping from configured spells to their light-emitting effects
        // This allows us to detect which configured spells are active by checking magic effects
        for (const auto& spellConfig : g_config.spells) {
            if (spellConfig.formId == 0) continue;

            auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellConfig.formId);
            if (!spell || spell->effects.size() == 0) {
                DebugPrint("Warning: Spell FormID 0x%08X not found or has no effects", spellConfig.formId);
                continue;
            }

            // Find effects with associated lights
            for (auto* effect : spell->effects) {
                if (!effect || !effect->baseEffect) continue;
                auto* assocForm = effect->baseEffect->data.associatedForm;
                if (!assocForm) continue;

                auto* asLight = assocForm->As<RE::TESObjectLIGH>();
                if (asLight) {
                    DebugPrint("Mapped effect 0x%08X -> spell 0x%08X", effect->baseEffect->GetFormID(),
                               spellConfig.formId);
                }
            }
        }

        DebugPrint("Built effect-to-spell mapping for %zu configured spells", g_config.spells.size());
    }

}
