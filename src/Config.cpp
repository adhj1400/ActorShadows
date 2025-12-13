#include "Config.h"

#include <fstream>
#include <sstream>

#include "Helper.h"

namespace TorchShadowLimiter {

    Config g_config;

    void LoadConfig() {
        std::string iniPath = "Data/SKSE/Plugins/PlayerShadows.ini";
        std::ifstream file(iniPath);

        if (!file.is_open()) {
            // Use defaults if file doesn't exist
            ConsolePrint("PlayerShadows.ini not found, using defaults");
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
            if (key == "TorchLightNodeName") {
                g_config.torchLightNodeName = value;
            } else if (key == "TorchLightOffsetX") {
                g_config.torchLightOffsetX = std::stof(value);
            } else if (key == "TorchLightOffsetY") {
                g_config.torchLightOffsetY = std::stof(value);
            } else if (key == "TorchLightOffsetZ") {
                g_config.torchLightOffsetZ = std::stof(value);
            } else if (key == "ShadowLightLimit") {
                g_config.shadowLightLimit = std::stoi(value);
            } else if (key == "EnableDebug") {
                g_config.enableDebug = (value == "true" || value == "1" || value == "True" || value == "TRUE");
            } else if (key == "PollIntervalSeconds") {
                try {
                    g_config.pollIntervalSeconds = std::stoi(value);
                } catch (...) {
                    // Keep default
                }
            }
        }

        file.close();
        DebugPrint("PlayerShadows config loaded: NodeName=%s, Offset=(%.1f, %.1f, %.1f), ShadowLimit=%d, Debug=%s",
                   g_config.torchLightNodeName.c_str(), g_config.torchLightOffsetX, g_config.torchLightOffsetY,
                   g_config.torchLightOffsetZ, g_config.shadowLightLimit, g_config.enableDebug ? "ON" : "OFF");
    }

}
