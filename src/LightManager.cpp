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

    void ForceReequipLight(RE::PlayerCharacter* player, bool wantShadows) {
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
                    if (g_originalLightTypes.find(lightFormId) != g_originalLightTypes.end()) {
                        SetLightTypeNative(torchBase, g_originalLightTypes[lightFormId]);
                        DebugPrint("Restored base form to original type: %u", g_originalLightTypes[lightFormId]);
                    }
                    g_isReequipping = false;
                });
            } else {
                g_isReequipping = false;
            }
        }).detach();
        if (wantShadows) {
            std::thread([]() {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(200ms);

                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([]() {
                        auto* pl = RE::PlayerCharacter::GetSingleton();
                        if (pl) {
                            AdjustHeldLightPosition(pl);
                        }
                    });
                }
            }).detach();
        }
    }

    namespace {
        // Recursively find all nodes with the given name, needed in cases that nodes are duplicated
        // e.g., multiple candlelight spells floating orb while re-casting
        void FindAllNodesByName(RE::NiAVObject* root, const std::string& name, std::vector<RE::NiAVObject*>& results) {
            if (!root) return;

            if (root->name == name.c_str()) {
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
                FindAllNodesByName(model3D, rootNodeName, rootNodes);

                if (rootNodes.empty()) {
                    DebugPrint("Root node '%s' not found for %s 0x%08X in %s person view", rootNodeName.c_str(),
                               itemType, formId, viewName);
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
                    DebugPrint(
                        "Adjusted %d light node(s) '%s' within %zu root node(s) '%s' for %s 0x%08X in %s "
                        "person view with values offset(%.2f, %.2f, %.2f) rotation(%.2f, %.2f, %.2f)",
                        adjustedCount, lightNodeName.c_str(), rootNodes.size(), rootNodeName.c_str(), itemType, formId,
                        viewName, offsetX, offsetY, offsetZ, rotateX, rotateY, rotateZ);
                    totalAdjusted += adjustedCount;
                }
            }

            if (totalAdjusted == 0) {
                DebugPrint("Light node '%s' not found in any model for %s 0x%08X", lightNodeName.c_str(), itemType,
                           formId);
            }
        }
    }

    void AdjustHeldLightPosition(RE::PlayerCharacter* player) {
        if (!player) return;

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
            DebugPrint("No configuration found for hand-held light 0x%08X", lightFormId);
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
            DebugPrint("No configuration found for spell 0x%08X", spellFormId);
            return;
        }

        AdjustLightNodePosition(player, spellConfig->rootNodeName, spellConfig->lightNodeName, spellConfig->offsetX,
                                spellConfig->offsetY, spellConfig->offsetZ, spellConfig->rotateX, spellConfig->rotateY,
                                spellConfig->rotateZ, spellFormId, "spell");
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

}
