#include "UpdateLogic.h"

#include <chrono>
#include <thread>

#include "Globals.h"
#include "LightHelpers.h"
#include "SKSE/SKSE.h"
#include "ShadowCounter.h"
#include "TorchManager.h"

namespace TorchShadowLimiter {

    void UpdateTorchShadowState_Native() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        // Count shadow lights
        int shadowLightCount = CountNearbyShadowLights();

        // Decide on desired state
        bool wantShadows = (shadowLightCount < 4);

        // ============ Torch Logic ============
        if (g_pollTorch) {
            auto* torchBase = GetPlayerTorchBase(player);
            if (torchBase) {
                // Only update if state changed from last known state
                if (wantShadows != g_lastShadowEnabled) {
                    if (auto* console = RE::ConsoleLog::GetSingleton()) {
                        console->Print("Torch shadow state changed: %s (shadow lights: %d)",
                                       wantShadows ? "ENABLE" : "DISABLE", shadowLightCount);
                    }

                    // Get current light type to remember original
                    uint8_t currentLightType = static_cast<uint8_t>(GetLightType(torchBase));

                    // Remember the original type if we're about to change it
                    if (g_originalTorchLightType == 255) {
                        g_originalTorchLightType = currentLightType;
                    }

                    // Modify the base form
                    uint8_t newType = wantShadows ? static_cast<uint8_t>(LightType::OmniShadow)
                                                  : static_cast<uint8_t>(LightType::OmniNS);
                    SetLightTypeNative(torchBase, newType);

                    // Force re-equip to update the reference
                    ForceReequipTorch(player);

                    // Adjust light position after re-equip
                    std::thread([]() {
                        using namespace std::chrono_literals;
                        std::this_thread::sleep_for(200ms);

                        if (auto* tasks = SKSE::GetTaskInterface()) {
                            tasks->AddTask([]() {
                                auto* pl = RE::PlayerCharacter::GetSingleton();
                                if (pl) {
                                    AdjustTorchLightPosition(pl);
                                }
                            });
                        }
                    }).detach();

                    g_lastShadowEnabled = wantShadows;
                }
            }
        }

        // ============ Candlelight Logic ============
        if (g_pollCandlelight) {
            // Check if Candlelight is active
            bool candlelightActive = false;

            auto* magicTarget = player->GetMagicTarget();
            if (magicTarget) {
                auto* activeEffects = magicTarget->GetActiveEffectList();
                if (activeEffects) {
                    for (auto& effect : *activeEffects) {
                        if (effect && effect->effect && effect->effect->baseEffect) {
                            if (effect->effect->baseEffect->GetFormID() == kCandlelightEffect) {
                                candlelightActive = true;
                                break;
                            }
                        }
                    }
                }
            }

            if (candlelightActive) {
                // Get Candlelight light base form
                auto* candlelightLight = RE::TESForm::LookupByID<RE::TESObjectLIGH>(kCandlelightLight);
                if (candlelightLight) {
                    // Only update if state changed from last known state
                    if (wantShadows != g_lastCandlelightShadowEnabled) {
                        if (auto* console = RE::ConsoleLog::GetSingleton()) {
                            console->Print("Candlelight shadow state changed: %s (shadow lights: %d)",
                                           wantShadows ? "ENABLE" : "DISABLE", shadowLightCount);
                        }

                        // Get current light type to remember original
                        uint8_t currentLightType = static_cast<uint8_t>(GetLightType(candlelightLight));

                        // Remember the original type
                        if (g_originalCandlelightLightType == 255) {
                            g_originalCandlelightLightType = currentLightType;
                        }

                        // Modify the base form
                        uint8_t newType = wantShadows ? static_cast<uint8_t>(LightType::OmniShadow)
                                                      : static_cast<uint8_t>(LightType::OmniNS);
                        SetLightTypeNative(candlelightLight, newType);

                        // Recast Candlelight
                        auto* candlelightSpell = RE::TESForm::LookupByID<RE::SpellItem>(kCandlelightSpell);
                        if (candlelightSpell) {
                            player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)
                                ->CastSpellImmediate(candlelightSpell, false, player, 1.0f, false, 0.0f, nullptr);
                        }

                        // Restore base form after delay
                        std::thread([candlelightLight]() {
                            using namespace std::chrono_literals;
                            std::this_thread::sleep_for(1s);

                            if (auto* tasks = SKSE::GetTaskInterface()) {
                                tasks->AddTask([candlelightLight]() {
                                    SetLightTypeNative(candlelightLight, g_originalCandlelightLightType);

                                    if (auto* console = RE::ConsoleLog::GetSingleton()) {
                                        console->Print("Restored Candlelight base form to original type: %u",
                                                       g_originalCandlelightLightType);
                                    }
                                });
                            }
                        }).detach();

                        g_lastCandlelightShadowEnabled = wantShadows;
                    }
                }
            }
        }
    }

    void StartTorchPollThread() {
        std::thread([]() {
            using namespace std::chrono_literals;

            while (true) {
                std::this_thread::sleep_for(5s);

                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([]() { UpdateTorchShadowState_Native(); });
                }
            }
        }).detach();

        if (auto* console = RE::ConsoleLog::GetSingleton()) {
            console->Print("Torch poll thread started (5s interval)");
        }
    }

}
