#include "LightManager.h"

#include <chrono>
#include <thread>

#include "Config.h"
#include "Globals.h"
#include "SKSE/SKSE.h"
#include "utils/Console.h"
#include "utils/Light.h"

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

        // Light is always in left hand
        RE::BGSEquipSlot* slot = nullptr;
        if (auto* dom = RE::BGSDefaultObjectManager::GetSingleton()) {
            slot = dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kLeftHandEquip);
        }

        g_isReequipping = true;

        uint32_t lightFormId = torchBase->GetFormID();

        equipManager->UnequipObject(player, torchBase, nullptr, 1, slot, true, false, false, false, nullptr);
        equipManager->EquipObject(player, torchBase, nullptr, 1, slot, true, false, false, false);

        // Restore base form after a delay so the reference keeps shadows but base form doesn't
        std::thread([torchBase, lightFormId]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(2s);  // Longer delay to ensure reference is fully created with shadows

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
    }

    namespace {
        void AdjustLightNodePosition(RE::PlayerCharacter* player, const std::string& rootNodeName,
                                     const std::string& lightNodeName, float offsetX, float offsetY, float offsetZ,
                                     float rotateX, float rotateY, float rotateZ, uint32_t formId,
                                     const char* itemType) {
            if (!player) return;

            if (rootNodeName.empty() || lightNodeName.empty()) return;

            // Check camera state to determine which 3D model to use
            auto* camera = RE::PlayerCamera::GetSingleton();
            bool isFirstPerson = camera && camera->IsInFirstPerson();

            // Get the appropriate 3D model based on camera state
            auto* model3D = player->Get3D(isFirstPerson);
            if (!model3D) {
                DebugPrint("3D model not found for %s 0x%08X in %s person view", itemType, formId,
                           isFirstPerson ? "first" : "third");
                return;
            }

            // Find the root node
            RE::NiAVObject* rootNode = model3D->GetObjectByName(rootNodeName.c_str());
            if (!rootNode) {
                DebugPrint("Root node '%s' not found for %s 0x%08X in %s person view", rootNodeName.c_str(), itemType,
                           formId, isFirstPerson ? "first" : "third");
                return;
            }

            // Then search for the light node within the root node
            RE::NiAVObject* lightNode = rootNode->GetObjectByName(lightNodeName.c_str());
            if (lightNode) {
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
                DebugPrint(
                    "Adjusted light node '%s' within root '%s' for %s 0x%08X with values offset(%.2f, %.2f, %.2f) "
                    "rotation(%.2f, %.2f, %.2f)",
                    lightNodeName.c_str(), rootNodeName.c_str(), itemType, formId, offsetX, offsetY, offsetZ, rotateX,
                    rotateY, rotateZ);
            } else {
                DebugPrint("Light node '%s' not found within root '%s' for %s 0x%08X", lightNodeName.c_str(),
                           rootNodeName.c_str(), itemType, formId);
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

}
