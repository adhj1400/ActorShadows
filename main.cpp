#include "SKSE/SKSE.h"
#include "src/Config.h"
#include "src/Globals.h"
#include "src/UpdateLogic.h"
#include "src/listeners/CellListener.h"
#include "src/listeners/EquipListener.h"
#include "src/listeners/SpellCastListener.h"
#include "src/utils/Console.h"

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

            StartShadowPollThread();
        }
    });

    return true;
}
