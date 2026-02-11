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
#include "utils/Transforms.h"

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

    void ForceCastSpell(RE::Actor* actor, RE::SpellItem* spell, bool withShadows) {
        if (!actor || !spell) {
            return;
        }
        auto actorFormId = actor->GetFormID();
        auto* light = spell->effects[0]->baseEffect->data.associatedForm->As<RE::TESObjectLIGH>();
        if (!light) {
            DebugPrint("ERROR", "Associated form for spell 0x%08X is not a light. Cannot cast.", spell->GetFormID());
            return;
        }
        SetLightTypeNative(
            light, withShadows ? static_cast<uint8_t>(LightType::OmniShadow) : static_cast<uint8_t>(LightType::OmniNS));

        auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        if (!caster) {
            return;
        }

        auto* trackedActor = ActorTracker::GetSingleton().GetOrCreateActor(actorFormId);
        if (!trackedActor) {
            DebugPrint("ERROR", "Failed to get or create tracked actor for actor 0x%08X. Cannot cast spell 0x%08X.",
                       actorFormId, spell->GetFormID());
            return;
        }

        trackedActor->SetLightShadowState(spell->GetFormID(), withShadows);
        caster->CastSpellImmediate(spell, false, actor, 1.0f, false, 0.0f, nullptr);

        // Restore base form after a delay so the reference keeps shadows but base form doesn't
        std::thread([light, actorFormId, spellFormId = spell->GetFormID()]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1000ms);  // Longer delay to ensure reference is fully created with shadows

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([light, actorFormId, spellFormId]() {
                    // Restore the base form
                    SetLightTypeNative(light, static_cast<uint8_t>(LightType::OmniNS));

                    // Re-fetch actor pointer to ensure it's valid
                    if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormId)) {
                        AdjustSpellLightPosition(actor, spellFormId);
                    }
                });
            }
        }).detach();
    }

    /**
     * Re-equips the item and restores the base form.
     * In charge of:
     * 1. Modifying the tracked actors re-equip state and light shadow state
     * 2. Modifying the base form to the correct shadow/no-shadow type
     */
    void ForceReEquipLight(RE::Actor* actor, RE::TESObjectLIGH* light, bool withShadows) {
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
                        AdjustHeldLightPosition(actor, lightFormId);
                    }

                    // Re-fetch to ensure valid pointer
                    if (auto* trackedActor = ActorTracker::GetSingleton().GetActor(actorFormId)) {
                        trackedActor->SetReEquipping(false);
                    }
                });
            }
        }).detach();
    }

    void ForceReEquipArmor(RE::Actor* actor, RE::TESObjectARMO* armor, bool withShadows) {
        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return;
        }

        TrackedActor* trackedActor = ActorTracker::GetSingleton().GetActor(actor->GetFormID());
        trackedActor->SetReEquipping(true);
        trackedActor->SetLightShadowState(armor->GetFormID(), withShadows);

        auto* armorLight = GetLightFromEnchantedArmor(armor);
        SetLightTypeNative(armorLight, static_cast<uint8_t>(withShadows ? LightType::OmniShadow : LightType::OmniNS));

        // Do entire sequence in one thread with delays between operations
        std::thread([actor, armor, armorLight, equipManager, withShadows, trackedActor]() {
            using namespace std::chrono_literals;
            constexpr auto unequipWaitTime = 600ms;
            constexpr auto enchantmentRespawnTime = 600ms;
            uint32_t armorFormId = armor->GetFormID();

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
                tasks->AddTask([armorLight, armorFormId, trackedActor]() {
                    if (armorLight) {
                        SetLightTypeNative(armorLight, static_cast<uint8_t>(LightType::OmniNS));
                    }
                    trackedActor->SetReEquipping(false);
                });
            }
        }).detach();
    }

    /*
     * Scan for any configured spells currently active on the actor
     */
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

    /**
     * Check if actor has a configured hand-held light equipped
     */
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

    /*
     * Get all equipped enchanted armors from config
     */
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

    /**
     * Get the associated light from an enchanted armors enchantment.
     */
    RE::TESObjectLIGH* GetLightFromEnchantedArmor(RE::TESObjectARMO* armor) {
        // Sanity checks
        if (!armor) {
            DebugPrint("WARN", "GetLightFromEnchantedArmor: armor is null");
            return nullptr;
        }
        if (!armor->formEnchanting) {
            DebugPrint("WARN", "Armor 0x%08X has no formEnchanting", armor->GetFormID());
            return nullptr;
        }
        auto* enchantment = armor->formEnchanting;
        if (enchantment->effects.size() == 0) {
            DebugPrint("WARN", "Armor 0x%08X enchantment has no effects", armor->GetFormID());
            return nullptr;
        }

        // Find the light-associated effect
        for (auto* effect : enchantment->effects) {
            // Effect sanity checks
            if (!effect || !effect->baseEffect) continue;

            auto* assocForm = effect->baseEffect->data.associatedForm;
            if (!assocForm) {
                DebugPrint("WARN", "Effect has no associated form");
                continue;
            }

            // Check if the associated form is a light
            auto* light = assocForm->As<RE::TESObjectLIGH>();
            if (light) {
                return light;
            }
        }

        DebugPrint("WARN", "No light found in armor 0x%08X enchantment", armor->GetFormID());
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

        // Get shadow distance from config (initialized from game INI settings)
        float shadowDistance = isInterior ? g_config.shadowDistanceInterior : g_config.shadowDistanceExterior;

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
