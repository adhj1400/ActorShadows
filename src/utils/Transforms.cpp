#include "Transforms.h"

#include "../core/Config.h"
#include "../utils/Console.h"

namespace ActorShadowLimiter {
    /**
     * Recursively find all nodes with the given name, needed in cases that nodes are
     * duplicated, e.g. multiple candlelight spells floating orb while re-casting.
     */
    void FindAllNodesByName(RE::NiAVObject* root, const std::string& name, std::vector<RE::NiAVObject*>& results) {
        if (!root) return;

        // Compare node name - handle both null and non-null names
        const char* nodeName = root->name.c_str();
        if (nodeName && std::string(nodeName) == name) {
            results.push_back(root);
        }

        // If this is a node, search its children
        if (auto* node = root->AsNode()) {
            for (auto& child : node->GetChildren()) {
                if (child) {
                    FindAllNodesByName(child.get(), name, results);
                }
            }
        }
    }

    void AdjustLightNodePosition(RE::Actor* actor, const std::string& rootNodeName, const std::string& lightNodeName,
                                 float offsetX, float offsetY, float offsetZ, float rotateX, float rotateY,
                                 float rotateZ, uint32_t formId, const char* itemType) {
        if (!actor) return;
        if (rootNodeName.empty() || lightNodeName.empty()) return;

        int totalAdjusted = 0;

        // Apply to both first person and third person models
        auto* firstPerson3D = actor->Get3D(true);
        auto* thirdPerson3D = actor->Get3D(false);

        for (int modelIndex = 0; modelIndex < 2; ++modelIndex) {
            auto* model3D = (modelIndex == 0) ? firstPerson3D : thirdPerson3D;
            if (!model3D) continue;

            const char* viewName = (modelIndex == 0) ? "first" : "third";

            // Find all root nodes with matching name (handles multiple nodes with same name, e.g., old + new)
            std::vector<RE::NiAVObject*> rootNodes;

            // Check if the model3D itself matches the root node name
            if (model3D->name == rootNodeName.c_str()) {
                rootNodes.push_back(model3D);
            }

            // Also search within the tree
            FindAllNodesByName(model3D, rootNodeName, rootNodes);

            if (rootNodes.empty()) {
                DebugPrint("TRANSFORM", "Root node '%s' not found for %s 0x%08X in %s person view",
                           rootNodeName.c_str(), itemType, formId, viewName);
                continue;
            }

            int adjustedCount = 0;
            // Apply transform to all light nodes within all root nodes
            for (auto* rootNode : rootNodes) {
                std::vector<RE::NiAVObject*> lightNodes;
                FindAllNodesByName(rootNode, lightNodeName, lightNodes);

                for (auto* lightNode : lightNodes) {
                    // Move the light (Y axis because node rotation is flipped)
                    lightNode->local.translate.x += offsetX;
                    lightNode->local.translate.y += offsetY;
                    lightNode->local.translate.z += offsetZ;

                    // Apply rotation (in radians)
                    constexpr float DEG_TO_RAD = 3.14159265f / 180.0f;
                    lightNode->local.rotate.SetEulerAnglesXYZ(rotateX * DEG_TO_RAD, rotateY * DEG_TO_RAD,
                                                              rotateZ * DEG_TO_RAD);

                    // Update the node
                    RE::NiUpdateData updateData;
                    lightNode->Update(updateData);
                    adjustedCount++;
                }
            }

            if (adjustedCount > 0) {
                DebugPrint("TRANSFORM",
                           "Adjusted %d light node(s) '%s' within %zu root node(s) '%s' for %s 0x%08X in %s "
                           "person view with values offset(%.2f, %.2f, %.2f) rotation(%.2f, %.2f, %.2f)",
                           adjustedCount, lightNodeName.c_str(), rootNodes.size(), rootNodeName.c_str(), itemType,
                           formId, viewName, offsetX, offsetY, offsetZ, rotateX, rotateY, rotateZ);
                totalAdjusted += adjustedCount;
            }
        }

        if (totalAdjusted == 0) {
            DebugPrint("TRANSFORM", "Light node '%s' not found in any model for %s 0x%08X", lightNodeName.c_str(),
                       itemType, formId);
        }
    }

    void AdjustHeldLightPosition(RE::Actor* actor, uint32_t lightFormId) {
        // Find the equipped light's config
        const HandHeldLightConfig* lightConfig = nullptr;

        for (const auto& config : g_config.handHeldLights) {
            if (config.formId == lightFormId) {
                lightConfig = &config;
                break;
            }
        }

        if (!lightConfig) {
            DebugPrint("TRANSFORM", "No configuration found for hand-held light 0x%08X", lightFormId);
            return;
        }

        AdjustLightNodePosition(actor, lightConfig->rootNodeName, lightConfig->lightNodeName, lightConfig->offsetX,
                                lightConfig->offsetY, lightConfig->offsetZ, lightConfig->rotateX, lightConfig->rotateY,
                                lightConfig->rotateZ, lightFormId, "light");
    }

    void AdjustSpellLightPosition(RE::Actor* actor, uint32_t spellFormId) {
        if (!actor) return;

        // Find the spell's config
        const SpellConfig* spellConfig = nullptr;

        for (const auto& config : g_config.spells) {
            if (config.formId == spellFormId) {
                spellConfig = &config;
                break;
            }
        }

        if (!spellConfig) {
            DebugPrint("TRANSFORM", "No configuration found for spell 0x%08X", spellFormId);
            return;
        }

        AdjustLightNodePosition(actor, spellConfig->rootNodeName, spellConfig->lightNodeName, spellConfig->offsetX,
                                spellConfig->offsetY, spellConfig->offsetZ, spellConfig->rotateX, spellConfig->rotateY,
                                spellConfig->rotateZ, spellFormId, "spell");
    }

    void AdjustEnchantmentLightPosition(RE::Actor* actor, uint32_t armorFormId) {
        if (!actor) return;
        const EnchantedArmorConfig* armorConfig = nullptr;

        for (const auto& config : g_config.enchantedArmors) {
            if (config.formId == armorFormId) {
                armorConfig = &config;
                break;
            }
        }

        if (!armorConfig) {
            DebugPrint("TRANSFORM", "No configuration found for enchanted armor 0x%08X", armorFormId);
            return;
        }

        AdjustLightNodePosition(actor, armorConfig->rootNodeName, armorConfig->lightNodeName, armorConfig->offsetX,
                                armorConfig->offsetY, armorConfig->offsetZ, armorConfig->rotateX, armorConfig->rotateY,
                                armorConfig->rotateZ, armorFormId, "armor");
    }
}
