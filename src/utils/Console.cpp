#include "Console.h"

#include <sstream>

#include "../core/Config.h"

namespace ActorShadowLimiter {
    void InitializeLog() {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }

        *path /= "ActorShadows.log"sv;
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

        auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("%g(%#): [%l] %v"s);
    }

    void DebugPrint(const std::string& action, const char* format, ...) {
        if (!g_config.enableDebug) return;

        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        SKSE::log::info("[{}] {}", action, buffer);
    }

    void DebugPrint(const std::string& action, RE::Actor* actor, const char* format, ...) {
        if (!g_config.enableDebug) return;

        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        std::string actorInfo = "<null>";
        if (actor) {
            const char* actorName = actor->GetName();
            uint32_t actorFormId = actor->GetFormID();
            char formIdBuf[16];
            snprintf(formIdBuf, sizeof(formIdBuf), "0x%08X", actorFormId);
            actorInfo = actorName ? actorName : "<unnamed>";
            actorInfo += " (";
            actorInfo += formIdBuf;
            actorInfo += ")";
        }

        SKSE::log::info("[{}] ['{}'] {}", action, actorInfo, buffer);
    }

    void PrintPlayerNiNodeTree() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            DebugPrint("NITREE", "Player singleton not found");
            return;
        }

        auto printNodeRecursive = [](RE::NiAVObject* node, int depth, auto& self) -> void {
            if (!node) return;

            std::string indent(depth * 2, ' ');
            std::string nodeName = node->name.c_str() ? node->name.c_str() : "<unnamed>";
            std::string nodeType = "NiAVObject";

            if (node->AsNode()) {
                nodeType = "NiNode";
            }

            DebugPrint("NITREE", "%s%s [%s] (0x%p)", indent.c_str(), nodeName.c_str(), nodeType.c_str(), node);

            // If this is a node, recurse through children
            if (auto* niNode = node->AsNode()) {
                for (auto& child : niNode->GetChildren()) {
                    if (child) {
                        self(child.get(), depth + 1, self);
                    }
                }
            }
        };

        DebugPrint("NITREE", "=== Player NiNode Tree (First Person) ===");
        auto* firstPerson3D = player->Get3D(true);
        if (firstPerson3D) {
            printNodeRecursive(firstPerson3D, 0, printNodeRecursive);
        } else {
            DebugPrint("NITREE", "First person 3D model not found");
        }

        DebugPrint("NITREE", "");
        DebugPrint("NITREE", "=== Player NiNode Tree (Third Person) ===");
        auto* thirdPerson3D = player->Get3D(false);
        if (thirdPerson3D) {
            printNodeRecursive(thirdPerson3D, 0, printNodeRecursive);
        } else {
            DebugPrint("NITREE", "Third person 3D model not found");
        }
    }

    std::string FormatFormIdMessage(uint32_t formId, const std::string& message) {
        std::stringstream ss;
        ss << message << " 0x" << std::hex << std::uppercase << formId;
        return ss.str();
    }

}
