#include "SKSE/SKSE.h"
#include "src/EventListeners.h"
#include "src/Globals.h"
#include "src/UpdateLogic.h"

using namespace SKSE;
using namespace TorchShadowLimiter;

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("PlayerShadows.dll loaded");
            }

            EquipListener::Install();
            SpellCastListener::Install();
            CellListener::Install();

            StartTorchPollThread();
        }
    });

    return true;
}
