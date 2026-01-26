#include "ShadowAllocator.h"

#include "../Config.h"
#include "../Globals.h"
#include "../LightManager.h"
#include "ActorLight.h"
#include "ActorTracker.h"
#include "Console.h"
#include "Light.h"
#include "ShadowCounter.h"

namespace ActorShadowLimiter {
    namespace ShadowAllocator {

        void AllocateShadows() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return;

            auto* cell = player->GetParentCell();
            if (!cell) return;

            bool isExterior = cell->IsExteriorCell();

            // Check if we should process NPCs in this location
            if (isExterior && !g_config.enableNpcsExterior) {
                return;
            }
            if (!isExterior && !g_config.enableNpcsInterior) {
                return;
            }

            std::vector<ActorPriority> actorsWithLights;

            // Add player if they have configured items
            if (HasConfiguredItems(player)) {
                ActorPriority playerPriority;
                playerPriority.actor = player;
                playerPriority.distanceToPlayer = 0.0f;
                playerPriority.isPlayer = true;
                actorsWithLights.push_back(playerPriority);
            }

            // Get player position for distance calculation
            auto playerPos = player->GetPosition();

            // Add NPCs from tracking set
            for (uint32_t npcFormId : ActorTracker::GetSingleton().GetTrackedActors()) {
                auto* npc = RE::TESForm::LookupByID<RE::Actor>(npcFormId);
                if (!npc || !npc->Is3DLoaded()) {
                    continue;
                }

                // Check if NPC still has configured items (might have unequipped)
                if (!HasConfiguredItems(npc)) {
                    continue;
                }

                // Calculate distance to player
                auto npcPos = npc->GetPosition();
                float distance = playerPos.GetDistance(npcPos);

                ActorPriority npcPriority;
                npcPriority.actor = npc;
                npcPriority.distanceToPlayer = distance;
                npcPriority.isPlayer = false;
                actorsWithLights.push_back(npcPriority);
            }

            // Sort by priority (player first, then by distance)
            std::sort(actorsWithLights.begin(), actorsWithLights.end());

            int shadowsUsed = CountNearbyShadowLights();
            int maxShadows = g_config.shadowLightLimit;

            // Allocate shadows to actors in priority order
            for (size_t i = 0; i < actorsWithLights.size(); ++i) {
                auto& actorPriority = actorsWithLights[i];
                RE::Actor* actor = actorPriority.actor;

                // Check what configured items this actor has
                auto equippedLight = GetEquippedLight(actor);
                auto activeSpells = GetActiveConfiguredSpells(actor);
                auto activeArmors = GetActiveConfiguredEnchantedArmors(actor);

                int lightsNeeded = 0;
                if (equippedLight) lightsNeeded++;
                lightsNeeded += static_cast<int>(activeSpells.size());
                lightsNeeded += static_cast<int>(activeArmors.size());

                // Count how many shadows this actor currently has enabled
                auto& tracker = ActorTracker::GetSingleton();
                uint32_t actorFormId = actor->GetFormID();
                int actorCurrentShadows = 0;
                if (equippedLight && tracker.GetShadowState(actorFormId, equippedLight->GetFormID())) {
                    actorCurrentShadows++;
                }
                for (uint32_t spellId : activeSpells) {
                    if (tracker.GetShadowState(actorFormId, spellId)) actorCurrentShadows++;
                }
                for (uint32_t armorId : activeArmors) {
                    if (tracker.GetShadowState(actorFormId, armorId)) actorCurrentShadows++;
                }

                // Check if enabling this actor's shadows would exceed the limit
                // (current total - what this actor has + what this actor needs) <= limit
                int projectedTotal = shadowsUsed - actorCurrentShadows + lightsNeeded;
                bool shouldHaveShadows = (projectedTotal <= maxShadows);

                // Handle equipped light
                if (equippedLight) {
                    uint32_t lightFormId = equippedLight->GetFormID();
                    bool lastState = tracker.GetShadowState(actorFormId, lightFormId);

                    if (shouldHaveShadows != lastState) {
                        const char* actorName = actorPriority.isPlayer ? "Player" : actor->GetName();
                        if (!actorName) actorName = "<unnamed>";

                        if (shouldHaveShadows) {
                            // Need to enable shadows - requires re-equip
                            // For NPCs, always reequip to clean up ExtraLight duplicates from cell transitions
                            DebugPrint("ALLOC", "Enabling shadows for %s (0x%08X) light 0x%08X (lastState=%d)",
                                       actorName, actorFormId, lightFormId, lastState);
                            SetLightTypeNative(equippedLight, static_cast<uint8_t>(LightType::OmniShadow));
                            ForceReequipLight(actor);
                            tracker.SetShadowState(actorFormId, lightFormId, true);
                        } else {
                            // Need to disable shadows - just reset, no re-equip needed
                            DebugPrint("ALLOC", "Disabling shadows for %s (0x%08X) light 0x%08X (lastState=%d)",
                                       actorName, actorFormId, lightFormId, lastState);
                            ResetEquippedLightToNoShadow(actor);
                            tracker.SetShadowState(actorFormId, lightFormId, false);
                        }
                    }
                }

                // Handle spells
                for (uint32_t spellFormId : activeSpells) {
                    bool lastState = tracker.GetShadowState(actorFormId, spellFormId);

                    if (shouldHaveShadows != lastState) {
                        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellFormId);
                        if (spell && !spell->effects.empty()) {
                            auto* effect = spell->effects[0];
                            if (effect && effect->baseEffect) {
                                auto* light = effect->baseEffect->data.light;
                                if (light) {
                                    uint8_t newType = shouldHaveShadows ? static_cast<uint8_t>(LightType::OmniShadow)
                                                                        : static_cast<uint8_t>(LightType::OmniNS);
                                    SetLightTypeNative(light, newType);
                                    tracker.SetShadowState(actorFormId, spellFormId, shouldHaveShadows);
                                }
                            }
                        }
                    }
                }

                // Handle enchanted armors
                for (uint32_t armorFormId : activeArmors) {
                    bool lastState = tracker.GetShadowState(actorFormId, armorFormId);

                    if (shouldHaveShadows != lastState) {
                        auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormId);
                        if (armor) {
                            auto* armorLight = GetLightFromEnchantedArmor(armor);
                            if (armorLight) {
                                uint8_t newType = shouldHaveShadows ? static_cast<uint8_t>(LightType::OmniShadow)
                                                                    : static_cast<uint8_t>(LightType::OmniNS);
                                SetLightTypeNative(armorLight, newType);
                                ForceReequipArmor(actor, armor);
                                tracker.SetShadowState(actorFormId, armorFormId, shouldHaveShadows);
                            }
                        }
                    }
                }

                // Update shadow count for next iteration
                if (shouldHaveShadows) {
                    shadowsUsed = shadowsUsed - actorCurrentShadows + lightsNeeded;
                }
            }
        }

    }
}