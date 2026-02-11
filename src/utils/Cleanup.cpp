#include "Cleanup.h"

#include <atomic>
#include <cmath>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../actor/ActorTracker.h"
#include "../core/Config.h"
#include "../utils/Console.h"
#include "SKSE/SKSE.h"

namespace ActorShadowLimiter {
    // Global flag for duplicate removal thread
    static std::atomic<bool> g_duplicateRemovalThreadRunning{false};

    // Check if any tracked actors have shadows enabled
    static bool HasActorsWithShadows() {
        auto trackedActorIds = ActorTracker::GetSingleton().GetAllTrackedActorIds();
        for (uint32_t actorFormId : trackedActorIds) {
            auto* trackedActor = ActorTracker::GetSingleton().GetActor(actorFormId);
            if (trackedActor && trackedActor->HasTrackedLight()) {
                auto trackedLight = trackedActor->GetTrackedLight();
                if (trackedLight.has_value() && trackedActor->GetLightShadowState(trackedLight.value())) {
                    return true;
                }
            }
        }
        return false;
    }

    void StartDuplicateRemovalThread() {
        if (!g_config.enableDuplicateFix) return;

        bool expected = false;
        if (!g_duplicateRemovalThreadRunning.compare_exchange_strong(expected, true)) {
            return;  // Already running
        }

        std::thread([]() {
            using namespace std::chrono_literals;
            DebugPrint("DUPLICATE", "Duplicate removal thread started (%dms interval)",
                       g_config.duplicateRemovalIntervalMs);

            while (g_duplicateRemovalThreadRunning) {
                std::this_thread::sleep_for(std::chrono::milliseconds(g_config.duplicateRemovalIntervalMs));

                if (!g_duplicateRemovalThreadRunning) {
                    break;
                }

                // Auto-stop if no tracked actors have shadows enabled
                if (!HasActorsWithShadows()) {
                    DebugPrint("DUPLICATE", "No tracked actors with shadows, stopping thread");
                    g_duplicateRemovalThreadRunning = false;
                    break;
                }

                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([]() { HideDuplicateLights(); });
                }
            }
            DebugPrint("DUPLICATE", "Duplicate removal thread stopped");
        }).detach();
    }

    void StopDuplicateRemovalThread() {
        if (!g_config.enableDuplicateFix) return;

        if (g_duplicateRemovalThreadRunning) {
            DebugPrint("DUPLICATE", "Stopping duplicate removal thread");
            g_duplicateRemovalThreadRunning = false;
        }
    }

    void HideDuplicateLights() {
        if (!g_config.enableDuplicateFix) return;

        // Get the shadow scene node
        auto* smState = &RE::BSShaderManager::State::GetSingleton();
        if (!smState) return;

        auto* shadowSceneNode = smState->shadowSceneNode[0];
        if (!shadowSceneNode) return;

        // Map: position key (rounded xyz) -> list of (bsLight, niLight, isShadowLight)
        std::unordered_map<std::string, std::vector<std::tuple<RE::BSLight*, RE::NiLight*, bool>>> lightsByPosition;

        auto scanLights = [&](const auto& lightList, bool isShadowList) {
            for (const auto& lightPtr : lightList) {
                if (auto* bsLight = lightPtr.get()) {
                    if (auto* niLight = bsLight->light.get()) {
                        // Get light world position
                        RE::NiPoint3 pos = niLight->world.translate;

                        // Create position key (round to nearest unit to handle floating point precision)
                        std::string posKey = std::to_string(static_cast<int>(std::round(pos.x))) + "_" +
                                             std::to_string(static_cast<int>(std::round(pos.y))) + "_" +
                                             std::to_string(static_cast<int>(std::round(pos.z)));

                        lightsByPosition[posKey].emplace_back(bsLight, niLight, isShadowList);
                    }
                }
            }
        };

        // Scan both shadow and non-shadow lights
        scanLights(shadowSceneNode->GetRuntimeData().activeLights, false);
        scanLights(shadowSceneNode->GetRuntimeData().activeShadowLights, true);

        // Remove duplicate BSLight entries from scene lists
        int duplicatesRemoved = 0;
        for (auto& [posKey, lights] : lightsByPosition) {
            if (lights.size() > 1) {
                DebugPrint("DUPLICATE", "Found %zu lights at position %s", lights.size(), posKey.c_str());
                // Keep shadow-casting lights, remove non-shadow duplicates
                for (size_t i = 0; i < lights.size(); ++i) {
                    auto& [bsLight, niLight, isShadowLight] = lights[i];

                    // Only remove non-shadow-casting duplicates
                    if (isShadowLight) {
                        continue;
                    }

                    // Remove non-shadow duplicate from activeLights array
                    auto& lightList = shadowSceneNode->GetRuntimeData().activeLights;
                    auto it = std::find_if(lightList.begin(), lightList.end(),
                                           [bsLight](const auto& ptr) { return ptr.get() == bsLight; });
                    if (it != lightList.end()) {
                        DebugPrint("DUPLICATE", "Removing non-shadow BSLight 0x%p at position %s", bsLight,
                                   posKey.c_str());
                        lightList.erase(it);
                        duplicatesRemoved++;
                    }
                }
            }
        }

        if (duplicatesRemoved > 0) {
            DebugPrint("DUPLICATE", "Removed %d duplicate light(s) from scene", duplicatesRemoved);
        }
    }

}