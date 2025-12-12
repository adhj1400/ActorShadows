#include "TorchManager.h"

#include <chrono>
#include <thread>

#include "Globals.h"
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

        // Which hand is it in?
        bool inLeft = (player->GetEquippedObject(true) == torchBase);
        bool inRight = (player->GetEquippedObject(false) == torchBase);

        if (!inLeft && !inRight) {
            return;
        }

        // Pick the slot based on hand
        RE::BGSEquipSlot* slot = nullptr;
        if (auto* dom = RE::BGSDefaultObjectManager::GetSingleton()) {
            if (inLeft) {
                slot = dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kLeftHandEquip);
            } else {
                slot = dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kRightHandEquip);
            }
        }

        g_isReequippingTorch = true;

        // Base form already modified by caller, just re-equip
        equipManager->UnequipObject(player, torchBase,
                                    nullptr,  // extraData
                                    1,        // count
                                    slot,     // equip slot
                                    true,     // queueEquip
                                    false,    // forceEquip
                                    false,    // playSound
                                    false,    // applyNow / locked
                                    nullptr   // unknown / disarm? (depends on version)
        );

        equipManager->EquipObject(player, torchBase,
                                  nullptr,  // extraData
                                  1, slot,
                                  true,    // queueEquip
                                  false,   // forceEquip
                                  false,   // playSound
                                  false);  // applyNow / locked

        // Restore base form after a delay so the reference keeps shadows but base form doesn't
        std::thread([torchBase]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(200ms);  // Short delay for torch to spawn

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([torchBase]() {
                    SetLightTypeNative(torchBase, g_originalTorchLightType);
                    g_isReequippingTorch = false;

                    if (auto* console = RE::ConsoleLog::GetSingleton()) {
                        console->Print("Restored base form to original type: %u", g_originalTorchLightType);
                    }
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

        RE::NiAVObject* lightNode = root3D->GetObjectByName("AttachLight");
        if (lightNode) {
            // Move the light upward (Y axis because node rotation is flipped)
            lightNode->local.translate.y += 20.0f;

            // Update the node
            RE::NiUpdateData updateData;
            lightNode->Update(updateData);
        }
    }

}  // namespace TorchShadowLimiter
