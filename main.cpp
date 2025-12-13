#include "SKSE/SKSE.h"
#include "src/Config.h"
#include "src/EventListeners.h"
#include "src/Globals.h"
#include "src/Helper.h"
#include "src/UpdateLogic.h"

using namespace SKSE;
using namespace TorchShadowLimiter;

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            LoadConfig();

            ConsolePrint("PlayerShadows.dll loaded");

            EquipListener::Install();
            SpellCastListener::Install();
            CellListener::Install();

            StartTorchPollThread();
        }
    });

    return true;
}
