#include <spdlog/sinks/basic_file_sink.h>

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

namespace {
    void InitializeLog() {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }

        *path /= "ActorShadows.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("%g(%#): [%l] %v");
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    InitializeLog();
    SKSE::log::info("ActorShadows loaded");

    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            LoadConfig();
            BuildEffectToSpellMapping();

            EquipListener::Install();
            SpellCastListener::Install();
            CellListener::Install();

            // Validate config items don't have shadows enabled
            CheckSanity();
        }
    });

    return true;
}
