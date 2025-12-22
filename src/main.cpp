#include "Config.h"
#include "Globals.h"
#include "SKSE/SKSE.h"
#include "UpdateLogic.h"
#include "listeners/CellListener.h"
#include "listeners/EquipListener.h"
#include "listeners/SpellCastListener.h"
#include "utils/Console.h"
#include "utils/Helpers.h"

using namespace SKSE;
using namespace ActorShadowLimiter;

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            LoadConfig();
            BuildEffectToSpellMapping();

            ConsolePrint("ActorShadows.dll loaded");

            EquipListener::Install();
            SpellCastListener::Install();
            CellListener::Install();

            // Validate config items don't have shadows enabled
            CheckSanity();
        }
    });

    return true;
}
