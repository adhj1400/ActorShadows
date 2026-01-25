#include "Helpers.h"

#include "../Config.h"
#include "../LightManager.h"
#include "Console.h"
#include "Light.h"

namespace ActorShadowLimiter {

    bool IsPlayer(RE::TESObjectREFR* ref) {
        if (!ref) {
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return false;
        }

        return ref->GetHandle() == player->GetHandle();
    }

    bool IsPlayerEquipEvent(const RE::TESEquipEvent* event) {
        if (!event || !event->actor) {
            return false;
        }
        return IsPlayer(event->actor.get());
    }

    bool IsPlayerSpellCastEvent(const RE::TESSpellCastEvent* event) {
        if (!event || !event->object) {
            return false;
        }
        return IsPlayer(event->object.get());
    }

    bool IsHandheldLight(RE::TESForm* form) { return form && form->GetFormType() == RE::FormType::Light; }

    bool IsLightEmittingArmor(RE::TESForm* form) { return form && form->GetFormType() == RE::FormType::Armor; }

    void WarnIfLightsHaveShadows() {
        bool foundShadows = false;

        // Check hand-held lights
        for (const auto& lightConfig : g_config.handHeldLights) {
            auto* light = RE::TESForm::LookupByID<RE::TESObjectLIGH>(lightConfig.formId);
            if (light && HasShadows(light)) {
                foundShadows = true;
                break;
            }
        }

        // Check spells
        if (!foundShadows) {
            for (const auto& spellConfig : g_config.spells) {
                auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellConfig.formId);
                if (spell && spell->effects.size() > 0) {
                    for (auto* effect : spell->effects) {
                        if (!effect || !effect->baseEffect) continue;
                        auto* assocForm = effect->baseEffect->data.associatedForm;
                        if (!assocForm) continue;

                        auto* spellLight = assocForm->As<RE::TESObjectLIGH>();
                        if (spellLight && HasShadows(spellLight)) {
                            foundShadows = true;
                            break;
                        }
                    }
                    if (foundShadows) break;
                }
            }
        }

        // Check enchanted armors
        if (!foundShadows) {
            for (const auto& armorConfig : g_config.enchantedArmors) {
                auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(armorConfig.formId);
                if (armor) {
                    auto* armorLight = GetLightFromEnchantedArmor(armor);
                    if (armorLight && HasShadows(armorLight)) {
                        foundShadows = true;
                        break;
                    }
                }
            }
        }

        // Display warning if any issues found
        if (foundShadows) {
            RE::DebugMessageBox(
                "ActorShadows Configuration Warning:\n\n"
                "One or more configured items have shadow flags enabled in their base forms.\n"
                "This may cause issues. Please disable shadows in CK/xEdit.\n\n"
                "Check ActorShadows.log for details.");
        }
    }

    void LogUnrestoredLights() {
        std::vector<std::pair<uint32_t, RE::TESObjectLIGH*>> configuredLights;

        // Collect held lights
        for (const auto& lightConfig : g_config.handHeldLights) {
            auto* lightBase = RE::TESForm::LookupByID<RE::TESObjectLIGH>(lightConfig.formId);
            if (lightBase) {
                configuredLights.push_back({lightConfig.formId, lightBase});
            }
        }

        // Collect spell lights
        for (const auto& spellConfig : g_config.spells) {
            auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(spellConfig.formId);
            if (spell && !spell->effects.empty()) {
                for (auto* effect : spell->effects) {
                    if (effect && effect->baseEffect) {
                        auto* assocForm = effect->baseEffect->data.associatedForm;
                        if (assocForm) {
                            auto* spellLightBase = assocForm->As<RE::TESObjectLIGH>();
                            if (spellLightBase) {
                                configuredLights.push_back({spellConfig.formId, spellLightBase});
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Collect armor lights
        for (const auto& armorConfig : g_config.enchantedArmors) {
            auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(armorConfig.formId);
            if (armor) {
                auto* armorLight = GetLightFromEnchantedArmor(armor);
                if (armorLight) {
                    configuredLights.push_back({armorConfig.formId, armorLight});
                }
            }
        }

        // Check all configured lights
        for (const auto& [formId, lightBase] : configuredLights) {
            if (HasShadows(lightBase)) {
                DebugPrint("UPDATE", "WARNING: Light 0x%08X base form still has shadows (not restored)", formId);
            }
        }
    }

}
