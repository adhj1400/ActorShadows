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

    void AdjustLightPosition(RE::PlayerCharacter* player) {
        if (!player) return;

        auto* root3D = player->Get3D(false);
        if (!root3D) return;

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

        if (!lightConfig) return;

        RE::NiAVObject* lightNode = root3D->GetObjectByName(lightConfig->nodeName.c_str());
        if (lightNode) {
            // Move the light (Y axis because node rotation is flipped)
            lightNode->local.translate.x += lightConfig->offsetX;
            lightNode->local.translate.y += lightConfig->offsetY;
            lightNode->local.translate.z += lightConfig->offsetZ;

            // Update the node
            RE::NiUpdateData updateData;
            lightNode->Update(updateData);
        }
    }

}
