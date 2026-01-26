#include "LightManager.h"

#include <chrono>
#include <thread>

#include "Config.h"
#include "Globals.h"
#include "SKSE/SKSE.h"
#include "utils/ActorLight.h"
#include "utils/ActorTracker.h"
#include "utils/Console.h"
#include "utils/Helpers.h"
#include "utils/Light.h"
#include "utils/MagicEffect.h"

namespace ActorShadowLimiter {

    void ResetEquippedLightToNoShadow(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        auto* lightBase = GetEquippedLight(actor);
        if (!lightBase) {
            return;
        }

        uint32_t lightFormId = lightBase->GetFormID();

        // Check if this light is configured
        bool isConfigured = false;
        for (const auto& config : g_config.handHeldLights) {
            if (config.formId == lightFormId) {
                isConfigured = true;
                break;
            }
        }

        if (!isConfigured) {
            return;
        }

        // Set to no-shadow variant
        uint8_t noShadowType = static_cast<uint8_t>(LightType::OmniNS);
        SetLightTypeNative(lightBase, noShadowType);

        // Update last known state
        ActorTracker::GetSingleton().SetShadowState(actor->GetFormID(), lightFormId, false);
    }

    void ResetActiveSpellsToNoShadow(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        // Get active configured spells
        auto activeSpells = GetActiveConfiguredSpells(actor);
        if (activeSpells.empty()) {
            return;
        }

        // For each active configured spell, reset its light to no-shadow
        for (uint32_t spellFormId : activeSpells) {
            // Get the spell form
            auto* spellForm = RE::TESForm::LookupByID(spellFormId);
            if (!spellForm) {
                continue;
            }

            auto* spell = spellForm->As<RE::SpellItem>();
            if (!spell || spell->effects.empty()) {
                continue;
            }

            // Get light from first effect
            auto* effect = spell->effects[0];
            if (!effect || !effect->baseEffect) {
                continue;
            }

            auto* lightBase = effect->baseEffect->data.light;
            if (!lightBase) {
                continue;
            }

            // Set to no-shadow variant
            uint8_t noShadowType = static_cast<uint8_t>(LightType::OmniNS);
            SetLightTypeNative(lightBase, noShadowType);

            // Update last known state
            ActorTracker::GetSingleton().SetShadowState(actor->GetFormID(), spellFormId, false);
        }
    }

    void ResetActiveEnchantedArmorsToNoShadow(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        // Get active configured enchanted armors
        auto activeArmors = GetActiveConfiguredEnchantedArmors(actor);
        if (activeArmors.empty()) {
            return;
        }

        // For each active configured enchanted armor, reset its light to no-shadow
        for (uint32_t armorFormId : activeArmors) {
            auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormId);
            if (!armor) {
                continue;
            }

            auto* armorLight = GetLightFromEnchantedArmor(armor);
            if (!armorLight) {
                continue;
            }

            // Set to no-shadow variant
            uint8_t noShadowType = static_cast<uint8_t>(LightType::OmniNS);
            SetLightTypeNative(armorLight, noShadowType);

            // Update last known state
            ActorTracker::GetSingleton().SetShadowState(actor->GetFormID(), armorFormId, false);
        }
    }

    void ForceReequipLight(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        uint32_t actorFormId = actor->GetFormID();
        auto& tracker = ActorTracker::GetSingleton();
        if (tracker.IsReequipping(actorFormId)) {
            return;
        }

        auto* torchBase = GetEquippedLight(actor);
        if (!torchBase) {
            return;
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return;
        }

        // Default to left hand slot (VR compatibility - GetObject crashes in VR)
        RE::BGSEquipSlot* slot = nullptr;

        tracker.StartReequip(actorFormId);

        uint32_t lightFormId = torchBase->GetFormID();

        // Try unequip/equip with null slot (works in both SE and VR)
        equipManager->UnequipObject(actor, torchBase, nullptr, 1, slot, true, false, false, false, nullptr);
        equipManager->EquipObject(actor, torchBase, nullptr, 1, slot, true, false, false, false);

        // Restore base form to OmniNS after delay (reference keeps shadows, base form is clean)
        // For NPCs: DON'T restore to prevent ExtraLight system from creating duplicate non-shadow lights
        std::thread([torchBase, lightFormId, actorFormId]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1000ms);  // Longer delay to ensure reference is fully created with shadows

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([torchBase, lightFormId, actorFormId]() {
                    bool isPlayer = (actorFormId == 0x14);

                    // Only restore base form for player (keeps inventory clean)
                    // For NPCs, leave as OmniS to prevent ExtraLight duplicates on cell transitions
                    if (isPlayer) {
                        SetLightTypeNative(torchBase, static_cast<uint8_t>(LightType::OmniNS));
                        DebugPrint("EQUIP", "Restored base form to OmniNS for player");
                    } else {
                        DebugPrint("EQUIP", "Left base form as OmniS for NPC 0x%08X (prevents ExtraLight duplicates)",
                                   actorFormId);
                    }

                    // Adjust light position if actor is still valid
                    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormId);
                    if (actor && actor->Is3DLoaded()) {
                        AdjustHeldLightPosition(actor);
                    }

                    ActorTracker::GetSingleton().EndReequip(actorFormId);
                });
            } else {
                ActorTracker::GetSingleton().EndReequip(actorFormId);
            }
        }).detach();
    }

    void ForceReequipArmor(RE::Actor* actor, RE::TESObjectARMO* armor) {
        if (!actor || !armor) {
            return;
        }

        uint32_t actorFormId = actor->GetFormID();
        auto& tracker = ActorTracker::GetSingleton();
        if (tracker.IsReequipping(actorFormId)) {
            return;
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return;
        }

        tracker.StartReequip(actorFormId);
        uint32_t armorFormId = armor->GetFormID();
        auto* armorLight = GetLightFromEnchantedArmor(armor);

        // Do entire sequence in one thread with delays between operations
        std::thread([actorFormId, armor, armorLight, armorFormId, equipManager]() {
            using namespace std::chrono_literals;
            constexpr auto unequipWaitTime = 100ms;
            constexpr auto enchantmentRespawnTime = 200ms;

            // Unequip
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([actorFormId, armor, equipManager]() {
                    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormId);
                    if (actor) {
                        equipManager->UnequipObject(actor, armor, nullptr, 1, nullptr, false, false, false, false,
                                                    nullptr);
                    }
                });
            }

            // Wait for unequip to complete
            std::this_thread::sleep_for(unequipWaitTime);

            // Re-equip
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([actorFormId, armor, equipManager]() {
                    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormId);
                    if (actor) {
                        equipManager->EquipObject(actor, armor, nullptr, 1, nullptr, false, false, false, false);
                    }
                });
            }

            // Wait for enchantment to respawn
            std::this_thread::sleep_for(enchantmentRespawnTime);

            // Restore base form
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([armorLight, armorFormId, actorFormId]() {
                    if (armorLight) {
                        SetLightTypeNative(armorLight, static_cast<uint8_t>(LightType::OmniNS));
                    }
                    ActorTracker::GetSingleton().EndReequip(actorFormId);
                });
            } else {
                ActorTracker::GetSingleton().EndReequip(actorFormId);
            }
        }).detach();
    }

    namespace {
        // Recursively find all nodes with the given name, needed in cases that nodes are duplicated
        // e.g., multiple candlelight spells floating orb while re-casting
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

        void AdjustLightNodePosition(RE::Actor* actor, const std::string& rootNodeName,
                                     const std::string& lightNodeName, float offsetX, float offsetY, float offsetZ,
                                     float rotateX, float rotateY, float rotateZ, uint32_t formId,
                                     const char* itemType) {
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
                        // Rename the light node to mark it as managed by ActorShadows
                        std::string newName = "ActorShadows_" + lightNodeName;
                        lightNode->name = newName.c_str();

                        DebugPrint("TRANSFORM", "Renamed '%s' to '%s' at 0x%p", lightNodeName.c_str(), newName.c_str(),
                                   lightNode);

                        // Also rename PtLight child if it exists
                        if (auto* lightNodeAsNode = lightNode->AsNode()) {
                            DebugPrint("TRANSFORM", "Light node is NiNode, searching for PtLight children");

                            // Debug: List all direct children
                            int childCount = 0;
                            for (auto& child : lightNodeAsNode->GetChildren()) {
                                if (child) {
                                    const char* childName = child->name.c_str();
                                    DebugPrint("TRANSFORM", "  Child %d: '%s' at 0x%p", childCount++,
                                               childName ? childName : "<unnamed>", child.get());
                                }
                            }

                            // Search for " PtLight" (note the leading space in vanilla name)
                            std::vector<RE::NiAVObject*> ptLights;
                            FindAllNodesByName(lightNodeAsNode, " PtLight", ptLights);
                            DebugPrint("TRANSFORM", "Found %zu PtLight node(s)", ptLights.size());

                            // Rename PtLight nodes to mark them as managed
                            for (auto* ptLight : ptLights) {
                                std::string ptLightNewName = "ActorShadows_Light";
                                ptLight->name = ptLightNewName.c_str();
                                DebugPrint("TRANSFORM", "Renamed PtLight to '%s' at 0x%p", ptLightNewName.c_str(),
                                           ptLight);
                            }
                        } else {
                            DebugPrint("TRANSFORM", "Light node '%s' is not NiNode, cannot search for PtLight children",
                                       lightNode->name.c_str());
                        }

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
    }

    void CleanupDuplicateLightNodes(RE::Actor* actor) {
        // Find the equipped light's config
        auto* lightBase = GetEquippedLight(actor);
        if (!lightBase) {
            return;
        }

        uint32_t lightFormId = lightBase->GetFormID();
        const HandHeldLightConfig* lightConfig = nullptr;

        for (const auto& config : g_config.handHeldLights) {
            if (config.formId == lightFormId) {
                lightConfig = &config;
                break;
            }
        }

        if (!lightConfig || lightConfig->lightNodeName.empty()) {
            return;
        }

        int totalDeleted = 0;
        auto* thirdPerson3D = actor->Get3D(false);
        if (thirdPerson3D) {
            std::vector<RE::NiAVObject*> lightNodes;
            FindAllNodesByName(thirdPerson3D, lightConfig->lightNodeName, lightNodes);

            // Only delete nodes that DON'T have the "ActorShadows_" prefix (vanilla duplicates)
            for (auto* lightNode : lightNodes) {
                if (!lightNode || !lightNode->parent) continue;

                const char* nodeName = lightNode->name.c_str();
                if (!nodeName) continue;

                // Check if this is a managed node (has ActorShadows_ prefix)
                bool isManagedNode = (std::string(nodeName).find("ActorShadows_") == 0);

                if (!isManagedNode) {
                    // This is a vanilla duplicate - remove it
                    DebugPrint("CLEANUP", "Removing vanilla duplicate '%s' at 0x%p for actor 0x%08X", nodeName,
                               lightNode, actor->GetFormID());
                    lightNode->parent->DetachChild(lightNode);
                    totalDeleted++;
                }
            }

            if (totalDeleted > 0) {
                DebugPrint("CLEANUP", "Deleted %d vanilla duplicate light node(s) for actor 0x%08X", totalDeleted,
                           actor->GetFormID());
            }
        }
    }

    void AdjustHeldLightPosition(RE::Actor* actor) {
        // Find the equipped light's config
        auto* lightBase = GetEquippedLight(actor);
        if (!lightBase) return;

        uint32_t lightFormId = lightBase->GetFormID();
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

        // Clean up any vanilla duplicate light nodes that may have been created during cell transitions
        CleanupDuplicateLightNodes(actor);
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

    // Get the light from an enchanted armor's enchantment
    RE::TESObjectLIGH* GetLightFromEnchantedArmor(RE::TESObjectARMO* armor) {
        if (!armor) {
            DebugPrint("ARMOR", "GetLightFromEnchantedArmor: armor is null");
            return nullptr;
        }

        DebugPrint("ARMOR", "Checking armor 0x%08X for enchantment", armor->GetFormID());

        if (!armor->formEnchanting) {
            DebugPrint("ARMOR", "Armor 0x%08X has no formEnchanting", armor->GetFormID());
            return nullptr;
        }

        auto* enchantment = armor->formEnchanting;
        DebugPrint("ARMOR", "Armor 0x%08X has enchantment 0x%08X with %zu effects", armor->GetFormID(),
                   enchantment->GetFormID(), enchantment->effects.size());

        if (enchantment->effects.size() == 0) {
            return nullptr;
        }

        // Find the light-associated effect
        for (auto* effect : enchantment->effects) {
            if (!effect || !effect->baseEffect) continue;

            DebugPrint("ARMOR", "Checking effect with base effect 0x%08X", effect->baseEffect->GetFormID());

            auto* assocForm = effect->baseEffect->data.associatedForm;
            if (!assocForm) {
                DebugPrint("ARMOR", "Effect has no associated form");
                continue;
            }

            DebugPrint("ARMOR", "Associated form 0x%08X, type: %d", assocForm->GetFormID(),
                       (int)assocForm->GetFormType());

            auto* light = assocForm->As<RE::TESObjectLIGH>();
            if (light) {
                DebugPrint("ARMOR", "Found light 0x%08X in armor enchantment", light->GetFormID());
                return light;
            }
        }

        DebugPrint("ARMOR", "No light found in armor 0x%08X enchantment", armor->GetFormID());
        return nullptr;
    }

}
