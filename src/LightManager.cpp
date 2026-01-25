#include "LightManager.h"

#include <chrono>
#include <thread>

#include "Config.h"
#include "Globals.h"
#include "SKSE/SKSE.h"
#include "utils/Console.h"
#include "utils/Light.h"
#include "utils/MagicEffect.h"

namespace ActorShadowLimiter {

    RE::TESObjectLIGH* GetEquippedLight(RE::PlayerCharacter* player) {
        if (!player) {
            return nullptr;
        }

        RE::TESForm* right = player->GetEquippedObject(false);
        RE::TESForm* left = player->GetEquippedObject(true);

        auto asLight = [](RE::TESForm* form) -> RE::TESObjectLIGH* {
            return form ? form->As<RE::TESObjectLIGH>() : nullptr;
        };

        if (auto* l = asLight(right)) {
            return l;
        }
        if (auto* l = asLight(left)) {
            return l;
        }

        return nullptr;
    }

    void ResetEquippedLightToNoShadow(RE::PlayerCharacter* player) {
        if (!player) {
            return;
        }

        auto* lightBase = GetEquippedLight(player);
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
        g_lastShadowStates[lightFormId] = false;
    }

    void ResetActiveSpellsToNoShadow(RE::PlayerCharacter* player) {
        if (!player) {
            return;
        }

        // Get active configured spells
        auto activeSpells = GetActiveConfiguredSpells(player);
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
            g_lastShadowStates[spellFormId] = false;
        }
    }

    void ResetActiveEnchantedArmorsToNoShadow(RE::PlayerCharacter* player) {
        if (!player) {
            return;
        }

        // Get active configured enchanted armors
        auto activeArmors = GetActiveConfiguredEnchantedArmors(player);
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
            g_lastShadowStates[armorFormId] = false;
        }
    }

    void ForceReequipLight(RE::PlayerCharacter* player) {
        if (g_isReequipping) {
            return;
        }
        if (!player || g_isReequipping) {
            return;
        }

        auto* torchBase = GetEquippedLight(player);
        if (!torchBase) {
            return;
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return;
        }

        // Default to left hand slot (VR compatibility - GetObject crashes in VR)
        RE::BGSEquipSlot* slot = nullptr;

        g_isReequipping = true;

        uint32_t lightFormId = torchBase->GetFormID();

        // Try unequip/equip with null slot (works in both SE and VR)
        equipManager->UnequipObject(player, torchBase, nullptr, 1, slot, true, false, false, false, nullptr);
        equipManager->EquipObject(player, torchBase, nullptr, 1, slot, true, false, false, false);

        // Restore base form after a delay so the reference keeps shadows but base form doesn't
        std::thread([torchBase, lightFormId]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(500ms);  // Longer delay to ensure reference is fully created with shadows

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([torchBase, lightFormId]() {
                    SetLightTypeNative(torchBase, static_cast<uint8_t>(LightType::OmniNS));
                    DebugPrint("EQUIP", "Restored base form to OmniNS");
                    g_isReequipping = false;
                });
            } else {
                g_isReequipping = false;
            }
        }).detach();
    }

    void ForceReequipArmor(RE::PlayerCharacter* player, RE::TESObjectARMO* armor) {
        if (!player || !armor || g_isReequipping) {
            return;
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return;
        }

        g_isReequipping = true;
        uint32_t armorFormId = armor->GetFormID();
        auto* armorLight = GetLightFromEnchantedArmor(armor);

        // Do entire sequence in one thread with delays between operations
        std::thread([player, armor, armorLight, armorFormId, equipManager]() {
            using namespace std::chrono_literals;
            constexpr auto unequipWaitTime = 100ms;
            constexpr auto enchantmentRespawnTime = 200ms;

            // Unequip
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([player, armor, equipManager]() {
                    equipManager->UnequipObject(player, armor, nullptr, 1, nullptr, false, false, false, false,
                                                nullptr);
                });
            }

            // Wait for unequip to complete
            std::this_thread::sleep_for(unequipWaitTime);

            // Re-equip
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([player, armor, equipManager]() {
                    equipManager->EquipObject(player, armor, nullptr, 1, nullptr, false, false, false, false);
                });
            }

            // Wait for enchantment to respawn
            std::this_thread::sleep_for(enchantmentRespawnTime);

            // Restore base form
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([armorLight, armorFormId]() {
                    if (armorLight) {
                        SetLightTypeNative(armorLight, static_cast<uint8_t>(LightType::OmniNS));
                    }
                    g_isReequipping = false;
                });
            } else {
                g_isReequipping = false;
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

        void AdjustLightNodePosition(RE::PlayerCharacter* player, const std::string& rootNodeName,
                                     const std::string& lightNodeName, float offsetX, float offsetY, float offsetZ,
                                     float rotateX, float rotateY, float rotateZ, uint32_t formId,
                                     const char* itemType) {
            if (!player) return;
            if (rootNodeName.empty() || lightNodeName.empty()) return;

            int totalAdjusted = 0;

            // Apply to both first person and third person models
            auto* firstPerson3D = player->Get3D(true);
            auto* thirdPerson3D = player->Get3D(false);

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
    }

    void AdjustHeldLightPosition(RE::PlayerCharacter* player) {
        // Find the equipped light's config
        auto* lightBase = GetEquippedLight(player);
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

        AdjustLightNodePosition(player, lightConfig->rootNodeName, lightConfig->lightNodeName, lightConfig->offsetX,
                                lightConfig->offsetY, lightConfig->offsetZ, lightConfig->rotateX, lightConfig->rotateY,
                                lightConfig->rotateZ, lightFormId, "light");
    }

    void AdjustSpellLightPosition(RE::PlayerCharacter* player, uint32_t spellFormId) {
        if (!player) return;

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

        AdjustLightNodePosition(player, spellConfig->rootNodeName, spellConfig->lightNodeName, spellConfig->offsetX,
                                spellConfig->offsetY, spellConfig->offsetZ, spellConfig->rotateX, spellConfig->rotateY,
                                spellConfig->rotateZ, spellFormId, "spell");
    }

    void AdjustEnchantmentLightPosition(RE::PlayerCharacter* player, uint32_t armorFormId) {
        if (!player) return;
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

        AdjustLightNodePosition(player, armorConfig->rootNodeName, armorConfig->lightNodeName, armorConfig->offsetX,
                                armorConfig->offsetY, armorConfig->offsetZ, armorConfig->rotateX, armorConfig->rotateY,
                                armorConfig->rotateZ, armorFormId, "armor");
    }

    // Scan for any configured spells currently active on the player
    std::vector<uint32_t> GetActiveConfiguredSpells(RE::PlayerCharacter* player) {
        std::vector<uint32_t> activeSpells;

        for (const auto& spellConfig : g_config.spells) {
            auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellConfig.formId);
            if (!spell || spell->effects.size() == 0) continue;

            // Check if any of this spell's effects are active
            for (auto* effect : spell->effects) {
                if (!effect || !effect->baseEffect) continue;

                if (HasMagicEffect(player, effect->baseEffect->GetFormID())) {
                    activeSpells.push_back(spellConfig.formId);
                    break;  // Found this spell active, move to next spell
                }
            }
        }

        return activeSpells;
    }

    // Check if player has a configured hand-held light equipped
    std::optional<uint32_t> GetActiveConfiguredLight(RE::PlayerCharacter* player) {
        auto* lightBase = GetEquippedLight(player);
        if (!lightBase) return std::nullopt;

        uint32_t lightFormId = lightBase->GetFormID();

        // Check if this light is in our configuration
        for (const auto& config : g_config.handHeldLights) {
            if (config.formId == lightFormId) {
                return lightFormId;
            }
        }

        return std::nullopt;
    }

    // Get all equipped enchanted armors from config
    std::vector<uint32_t> GetActiveConfiguredEnchantedArmors(RE::PlayerCharacter* player) {
        std::vector<uint32_t> activeArmors;
        if (!player) return activeArmors;

        auto inv = player->GetInventory();

        for (const auto& [item, data] : inv) {
            if (!item || !data.second) continue;
            if (!data.second->IsWorn()) continue;

            auto* armor = item->As<RE::TESObjectARMO>();
            if (!armor) continue;

            // Check if this armor is in our configuration
            for (const auto& config : g_config.enchantedArmors) {
                if (config.formId == armor->GetFormID()) {
                    activeArmors.push_back(armor->GetFormID());
                    break;
                }
            }
        }

        return activeArmors;
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
