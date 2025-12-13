#include "TorchManager.h"

#include <chrono>
#include <thread>

#include "Config.h"
#include "Globals.h"
#include "Helper.h"
#include "LightHelpers.h"
#include "SKSE/SKSE.h"

namespace TorchShadowLimiter {

    RE::TESObjectLIGH* GetPlayerTorchBase(RE::PlayerCharacter* player) {
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

    void ForceReequipTorch(RE::PlayerCharacter* player) {
        if (g_isReequippingTorch) {
            return;
        }
        if (!player || g_isReequippingTorch) {
            return;
        }

        auto* torchBase = GetPlayerTorchBase(player);
        if (!torchBase) {
            return;
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return;
        }

        // Torch is always in left hand
        RE::BGSEquipSlot* slot = nullptr;
        if (auto* dom = RE::BGSDefaultObjectManager::GetSingleton()) {
            slot = dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kLeftHandEquip);
        }

        g_isReequippingTorch = true;

        equipManager->UnequipObject(player, torchBase, nullptr, 1, slot, true, false, false, false, nullptr);
        equipManager->EquipObject(player, torchBase, nullptr, 1, slot, true, false, false, false);

        // Restore base form after a delay so the reference keeps shadows but base form doesn't
        std::thread([torchBase]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(2s);  // Longer delay to ensure reference is fully created with shadows

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([torchBase]() {
                    SetLightTypeNative(torchBase, g_originalTorchLightType);
                    g_isReequippingTorch = false;

                    DebugPrint("Restored base form to original type: %u", g_originalTorchLightType);
                });
            } else {
                g_isReequippingTorch = false;
            }
        }).detach();
    }

    void AdjustTorchLightPosition(RE::PlayerCharacter* player) {
        if (!player) return;

        auto* root3D = player->Get3D(false);
        if (!root3D) return;

        RE::NiAVObject* lightNode = root3D->GetObjectByName(g_config.torchLightNodeName.c_str());
        if (lightNode) {
            // Move the light (Y axis because node rotation is flipped)
            lightNode->local.translate.x += g_config.torchLightOffsetX;
            lightNode->local.translate.y += g_config.torchLightOffsetY;
            lightNode->local.translate.z += g_config.torchLightOffsetZ;

            // Update the node
            RE::NiUpdateData updateData;
            lightNode->Update(updateData);
        }
    }

}
