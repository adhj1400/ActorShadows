#include "SKSE/SKSE.h"
#include "UpdateLogic.h"
#include "core/Config.h"
#include "core/Globals.h"
#include "events/CellListener.h"
#include "events/EquipListener.h"
#include "events/SpellCastListener.h"
#include "utils/Console.h"
#include "utils/Helpers.h"

using namespace SKSE;
using namespace ActorShadowLimiter;

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    InitializeLog();
    SKSE::Init(skse);
    SKSE::log::info("ActorShadows loaded");

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            LoadConfig();

            EquipListener::Install();
            SpellCastListener::Install();
            CellListener::Install();

            WarnIfLightsHaveShadows();
        }
    });

    return true;
}
