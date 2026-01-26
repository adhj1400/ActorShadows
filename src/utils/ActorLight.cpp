#include "ActorLight.h"

#include "../Config.h"
#include "Console.h"
#include "Light.h"
#include "MagicEffect.h"

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

    void RemoveEquippedLightExtraData(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        // Get the actor's inventory
        auto* changes = actor->GetInventoryChanges();
        if (!changes || !changes->entryList) {
            return;
        }

        // Find the equipped light in inventory
        for (auto* entry : *changes->entryList) {
            if (!entry || !entry->object) continue;

            auto* light = entry->object->As<RE::TESObjectLIGH>();
            if (!light) continue;

            // Check if this light is equipped
            if (!entry->extraLists) continue;

            for (auto* extraList : *entry->extraLists) {
                if (!extraList) continue;

                // Check if equipped
                auto* worn = extraList->GetByType<RE::ExtraWorn>();
                auto* wornLeft = extraList->GetByType<RE::ExtraWornLeft>();
                if (!worn && !wornLeft) continue;

                // This is the equipped light - remove ExtraLight if present
                auto* extraLight = extraList->GetByType<RE::ExtraLight>();
                if (extraLight) {
                    extraList->Remove(extraLight);
                    DebugPrint("CLEANUP", "Removed ExtraLight from equipped torch for actor 0x%08X",
                               actor->GetFormID());
                    return;
                }
            }
        }
    }

    std::vector<uint32_t> GetActiveConfiguredSpells(RE::Actor* actor) {
        std::vector<uint32_t> activeSpells;

        for (const auto& spellConfig : g_config.spells) {
            auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellConfig.formId);
            if (!spell || spell->effects.empty()) continue;

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

    std::vector<uint32_t> GetActiveConfiguredEnchantedArmors(RE::Actor* actor) {
        std::vector<uint32_t> activeArmors;

        if (!actor) {
            return activeArmors;
        }

        for (const auto& armorConfig : g_config.enchantedArmors) {
            auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(armorConfig.formId);
            if (!armor) continue;

            // Check if this armor is equipped
            auto* extraContainerChanges = actor->extraList.GetByType<RE::ExtraContainerChanges>();
            if (!extraContainerChanges || !extraContainerChanges->changes) {
                continue;
            }

            bool isEquipped = false;
            if (extraContainerChanges->changes->entryList) {
                for (auto* entry : *extraContainerChanges->changes->entryList) {
                    if (entry && entry->object && entry->object->GetFormID() == armorConfig.formId) {
                        if (entry->extraLists) {
                            for (auto* extraList : *entry->extraLists) {
                                if (extraList && extraList->HasType(RE::ExtraDataType::kWorn)) {
                                    isEquipped = true;
                                    break;
                                }
                            }
                        }
                        if (isEquipped) break;
                    }
                }
            }

            if (isEquipped) {
                activeArmors.push_back(armorConfig.formId);
            }
        }

        return activeArmors;
    }

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

    bool HasConfiguredItems(RE::Actor* actor) {
        if (!actor) return false;

        // Check equipped light
        if (GetActiveConfiguredLight(actor).has_value()) {
            return true;
        }

        // Check active spells
        if (!GetActiveConfiguredSpells(actor).empty()) {
            return true;
        }

        // Check equipped armors
        if (!GetActiveConfiguredEnchantedArmors(actor).empty()) {
            return true;
        }

        return false;
    }

}
