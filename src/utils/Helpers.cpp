#include "Helpers.h"

#include <sstream>

#include "../Config.h"
#include "../LightManager.h"
#include "Console.h"
#include "Light.h"

namespace ActorShadowLimiter {

    bool IsPlayer(RE::TESObjectREFR* ref) {
        if (!ref) {
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return false;
        }

        return ref->GetHandle() == player->GetHandle();
    }

    void CheckSanity() {
        std::vector<std::string> warnings;

        // Check hand-held lights
        for (const auto& lightConfig : g_config.handHeldLights) {
            auto* light = RE::TESForm::LookupByID<RE::TESObjectLIGH>(lightConfig.formId);
            if (light && HasShadows(light)) {
                std::stringstream ss;
                ss << "Hand-held light 0x" << std::hex << std::uppercase << lightConfig.formId
                   << " has shadows enabled in base form";
                warnings.push_back(ss.str());
            }
        }

        // Check spells
        for (const auto& spellConfig : g_config.spells) {
            auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellConfig.formId);
            if (spell && spell->effects.size() > 0) {
                for (auto* effect : spell->effects) {
                    if (!effect || !effect->baseEffect) continue;
                    auto* assocForm = effect->baseEffect->data.associatedForm;
                    if (!assocForm) continue;

                    auto* spellLight = assocForm->As<RE::TESObjectLIGH>();
                    if (spellLight && HasShadows(spellLight)) {
                        std::stringstream ss;
                        ss << "Spell 0x" << std::hex << std::uppercase << spellConfig.formId
                           << " has light with shadows enabled in base form";
                        warnings.push_back(ss.str());
                        break;
                    }
                }
            }
        }

        // Check enchanted armors
        for (const auto& armorConfig : g_config.enchantedArmors) {
            auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(armorConfig.formId);
            if (armor) {
                auto* armorLight = GetLightFromEnchantedArmor(armor);
                if (armorLight && HasShadows(armorLight)) {
                    std::stringstream ss;
                    ss << "Enchanted armor 0x" << std::hex << std::uppercase << armorConfig.formId
                       << " has light with shadows enabled in base form";
                    warnings.push_back(ss.str());
                }
            }
        }

        // Display warning if any issues found
        if (!warnings.empty()) {
            std::stringstream message;
            message << "ActorShadows Configuration Warning:\n\n";
            message << "The following items have shadow flags enabled in their base forms.\n";
            message << "This may cause issues. Please disable shadows in CK/xEdit:\n\n";

            for (const auto& warning : warnings) {
                message << "- " << warning << "\n";
            }

            RE::DebugMessageBox(message.str().c_str());
        }
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

}
