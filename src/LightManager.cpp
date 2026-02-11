#include "LightManager.h"

#include <chrono>
#include <thread>

#include "SKSE/SKSE.h"
#include "actor/ActorTracker.h"
#include "core/Config.h"
#include "core/Globals.h"
#include "utils/Console.h"
#include "utils/Light.h"
#include "utils/MagicEffect.h"

namespace ActorShadowLimiter {

    RE::TESObjectLIGH* GetEquippedLight(RE::Actor* actor) {
        if (!actor) {
            return nullptr;
        }

        RE::TESForm* right = actor->GetEquippedObject(false);
        RE::TESForm* left = actor->GetEquippedObject(true);

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

        // Reset the base form
        SetLightTypeNative(lightBase, static_cast<uint8_t>(LightType::OmniNS));
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
            g_lastShadowStates[spellFormId] = false;
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
            g_lastShadowStates[armorFormId] = false;
        }
    }

    /**
     * Re-equips the item and restores the base form.
     * In charge of:
     * 1. Modifying the tracked actors re-equip state and light shadow state
     * 2. Modifying the base form to the correct shadow/no-shadow type
     */
    void ForceReequipLight(RE::Actor* actor, RE::TESObjectLIGH* light, bool withShadows) {
        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        TrackedActor* trackedActor = ActorTracker::GetSingleton().GetActor(actor->GetFormID());
        if (!equipManager || !trackedActor) {
            DebugPrint("Warn",
                       "Failed to get equip manager or tracked actor for actor 0x%08X. Cannot re-equip light 0x%08X.",
                       actor->GetFormID(), light->GetFormID());
            return;
        }

        trackedActor->SetReEquipping(true);
        trackedActor->SetLightShadowState(light->GetFormID(), withShadows);

        SetLightTypeNative(light, static_cast<uint8_t>(withShadows ? LightType::OmniShadow : LightType::OmniNS));

        // Default to left hand slot (VR compatibility - GetObject crashes in VR)
        RE::BGSEquipSlot* slot = nullptr;
        uint32_t lightFormId = light->GetFormID();
        uint32_t actorFormId = actor->GetFormID();

        // Try unequip/equip with null slot
        equipManager->UnequipObject(actor, light, nullptr, 1, slot, true, false, false, false, nullptr);
        equipManager->EquipObject(actor, light, nullptr, 1, slot, true, false, false, false);

        // Restore base form after a delay so the reference keeps shadows but base form doesn't
        std::thread([light, lightFormId, actorFormId]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1000ms);  // Longer delay to ensure reference is fully created with shadows

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([light, lightFormId, actorFormId]() {
                    // Restore the base form
                    SetLightTypeNative(light, static_cast<uint8_t>(LightType::OmniNS));

                    // Re-fetch actor pointer to ensure it's valid
                    if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormId)) {
                        AdjustHeldLightPosition(actor);
                    }

                    // Re-fetch to ensure valid pointer
                    if (auto* trackedActor = ActorTracker::GetSingleton().GetActor(actorFormId)) {
                        trackedActor->SetReEquipping(false);
                    }
                });
            }
        }).detach();
    }

    void ForceReequipArmor(RE::Actor* actor, RE::TESObjectARMO* armor) {
        if (!actor || !armor || g_isReequipping) {
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
        std::thread([actor, armor, armorLight, armorFormId, equipManager]() {
            using namespace std::chrono_literals;
            constexpr auto unequipWaitTime = 100ms;
            constexpr auto enchantmentRespawnTime = 200ms;

            // Unequip
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([actor, armor, equipManager]() {
                    equipManager->UnequipObject(actor, armor, nullptr, 1, nullptr, false, false, false, false, nullptr);
                });
            }

            // Wait for unequip to complete
            std::this_thread::sleep_for(unequipWaitTime);

            // Re-equip
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([actor, armor, equipManager]() {
                    equipManager->EquipObject(actor, armor, nullptr, 1, nullptr, false, false, false, false);
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

    // Scan for any configured spells currently active on the actor
    std::vector<uint32_t> GetActiveConfiguredSpells(RE::Actor* actor) {
        std::vector<uint32_t> activeSpells;

        for (const auto& spellConfig : g_config.spells) {
            auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellConfig.formId);
            if (!spell || spell->effects.size() == 0) continue;

            // Check if any of this spell's effects are active
            for (auto* effect : spell->effects) {
                if (!effect || !effect->baseEffect) continue;

                if (HasMagicEffect(actor, effect->baseEffect->GetFormID())) {
                    activeSpells.push_back(spellConfig.formId);
                    break;  // Found this spell active, move to next spell
                }
            }
        }

        return activeSpells;
    }

    // Check if actor has a configured hand-held light equipped
    std::optional<uint32_t> GetActiveConfiguredLight(RE::Actor* actor) {
        auto* lightBase = GetEquippedLight(actor);
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
    std::vector<uint32_t> GetActiveConfiguredEnchantedArmors(RE::Actor* actor) {
        std::vector<uint32_t> activeArmors;
        if (!actor) return activeArmors;

        auto inv = actor->GetInventory();

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

    /**
     * Count nearby shadow-casting lights.
     */
    int CountNearbyShadowLights() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return 0;
        }

        auto* cell = player->GetParentCell();
        if (!cell) {
            return 0;
        }
        bool isInterior = cell->IsInteriorCell();
        DebugPrint("DEBUG", "Starting cell scan on cell '%s'...", cell->GetFormEditorID());

        RE::NiPoint3 playerPos = player->GetPosition();

        // Get the shadow scene node - contains all actively rendered shadow lights
        auto* smState = &RE::BSShaderManager::State::GetSingleton();
        if (!smState) {
            return 0;
        }

        auto* shadowSceneNode = smState->shadowSceneNode[0];
        if (!shadowSceneNode) {
            return 0;
        }

        int shadowLightCount = 0;
        float closestLightDistance = std::numeric_limits<float>::max();

        // Track counted lights by position and radius to avoid counting first/third person duplicates
        struct CountedLight {
            RE::NiPoint3 pos;
            float radius;
        };
        std::vector<CountedLight> countedLights;

        // Get shadow distance from INI settings
        float shadowDistance = 3000.0f;  // Default fallback

        const char* settingName = isInterior ? "fInteriorShadowDistance:Display" : "fShadowDistance:Display";

        // Fetch SkyrimPrefs.ini settings for shadow render distance
        auto* prefSettings = RE::INIPrefSettingCollection::GetSingleton();
        if (prefSettings) {
            auto* setting = prefSettings->GetSetting(settingName);
            if (setting) {
                shadowDistance = setting->GetFloat();
                DebugPrint("SCAN", "Found %s in INIPrefSettingCollection: %.1f", settingName, shadowDistance);
            } else {
                DebugPrint("WARN", "Setting %s not found in INIPrefSettingCollection", settingName);
            }
        } else {
            DebugPrint("WARN", "Failed to get INIPrefSettingCollection singleton");
        }

        // Iterate through active shadow lights (already filtered by renderer)
        auto& activeShadowLights = shadowSceneNode->GetRuntimeData().activeShadowLights;
        for (const auto& lightPtr : activeShadowLights) {
            if (auto* bsLight = lightPtr.get()) {
                if (auto* niLight = bsLight->light.get()) {
                    // Get light world position
                    RE::NiPoint3 lightPos = niLight->world.translate;

                    // Calculate distance from player
                    float dx = lightPos.x - playerPos.x;
                    float dy = lightPos.y - playerPos.y;
                    float dz = lightPos.z - playerPos.z;
                    float distSq = dx * dx + dy * dy + dz * dz;

                    // Check if within search radius
                    float distance = std::sqrt(distSq);

                    // Get light radius from NiLight runtime data
                    auto& radiusVec = niLight->GetLightRuntimeData().radius;
                    float radius = radiusVec.x;

                    // Effective shadow distance: light radius + game's shadow distance setting + config modifier
                    float effectiveShadowDistance = radius + shadowDistance + g_config.shadowDistanceSafetyMargin;
                    bool withinEffectiveShadowDist = distance <= effectiveShadowDistance;

                    if (withinEffectiveShadowDist) {
                        // Check position clustering to avoid counting first/third person duplicates
                        bool isDuplicate = false;

                        for (const auto& counted : countedLights) {
                            float lx = lightPos.x - counted.pos.x;
                            float ly = lightPos.y - counted.pos.y;
                            float lz = lightPos.z - counted.pos.z;
                            float posDiff = std::sqrt(lx * lx + ly * ly + lz * lz);

                            // If within 50 units and same radius, consider it a duplicate
                            if (posDiff < 100.0f && std::abs(radius - counted.radius) < 0.1f) {
                                isDuplicate = true;
                                DebugPrint("SCAN",
                                           "Skipping duplicate light at (%.1f, %.1f, %.1f) - %.1f units from "
                                           "already counted light",
                                           lightPos.x, lightPos.y, lightPos.z, posDiff);
                                break;
                            }
                        }

                        if (!isDuplicate) {
                            countedLights.push_back({lightPos, radius});
                            DebugPrint("SCAN",
                                       "Found shadow light - Pos: (%.1f, %.1f, %.1f), Distance: %.1f, Radius: %.1f, "
                                       "EffectiveDist: %.1f",
                                       lightPos.x, lightPos.y, lightPos.z, distance, radius, effectiveShadowDistance);
                            ++shadowLightCount;
                            if (distance < closestLightDistance) {
                                closestLightDistance = distance;
                            }
                        }
                    }
                }
            }
        }

        DebugPrint("SCAN", "There are %d lights too close to the player of total %d, Type: %s", shadowLightCount,
                   activeShadowLights.size(), isInterior ? "INTERIOR" : "EXTERIOR");
        DebugPrint("DEBUG", "End of cell scan.");

        return shadowLightCount;
    }

}
