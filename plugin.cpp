#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

using namespace SKSE;
using namespace RE;

namespace {
    using FLAGS = RE::TES_LIGHT_FLAGS;

    bool g_lastShadowEnabled = false;
    std::atomic_bool g_pollTorch{false};

    // -------------------------------
    // Light Type Helpers
    // -------------------------------

    std::uint32_t GetLightType(const TESObjectLIGH* a_light) {
        if (!a_light) {
            return 0;
        }

        const auto& flags = a_light->data.flags;

        if (flags.none(FLAGS::kHemiShadow) && flags.none(FLAGS::kOmniShadow) && flags.none(FLAGS::kSpotlight) &&
            flags.none(FLAGS::kSpotShadow)) {
            return 2;  // Omni (no shadow)
        }
        if (flags.any(FLAGS::kHemiShadow)) {
            return 1;
        }
        if (flags.any(FLAGS::kOmniShadow)) {
            return 3;
        }
        if (flags.any(FLAGS::kSpotlight)) {
            return 4;
        }
        if (flags.any(FLAGS::kSpotShadow)) {
            return 5;
        }

        return 0;
    }

    void SetLightTypeNative(TESObjectLIGH* a_light, std::uint32_t a_type) {
        if (!a_light) {
            return;
        }

        auto& flags = a_light->data.flags;
        switch (a_type) {
            case 1:
                flags.reset(FLAGS::kOmniShadow, FLAGS::kSpotlight, FLAGS::kSpotShadow);
                flags.set(FLAGS::kHemiShadow);
                break;
            case 2:
                flags.reset(FLAGS::kHemiShadow, FLAGS::kOmniShadow, FLAGS::kSpotlight, FLAGS::kSpotShadow);
                break;
            case 3:
                flags.reset(FLAGS::kHemiShadow, FLAGS::kSpotlight, FLAGS::kSpotShadow);
                flags.set(FLAGS::kOmniShadow);
                break;
            case 4:
                flags.reset(FLAGS::kHemiShadow, FLAGS::kOmniShadow, FLAGS::kSpotShadow);
                flags.set(FLAGS::kSpotlight);
                break;
            case 5:
                flags.reset(FLAGS::kHemiShadow, FLAGS::kOmniShadow, FLAGS::kSpotlight);
                flags.set(FLAGS::kSpotShadow);
                break;
            default:
                break;
        }
    }

    // -------------------------------
    // Find Player Torch
    // -------------------------------

    TESObjectLIGH* GetPlayerTorchBase(PlayerCharacter* player) {
        if (!player) {
            return nullptr;
        }

        TESForm* right = player->GetEquippedObject(false);
        TESForm* left = player->GetEquippedObject(true);

        auto asLight = [](TESForm* form) -> TESObjectLIGH* { return form ? form->As<TESObjectLIGH>() : nullptr; };

        if (auto* l = asLight(right)) {
            return l;
        }
        if (auto* l = asLight(left)) {
            return l;
        }

        return nullptr;
    }

    bool g_isReequippingTorch = false;

    void ForceReequipTorch(RE::PlayerCharacter* player) {
        if (g_isReequippingTorch) {
            return;
        }
        if (!player || g_isReequippingTorch) {
            return;
        }

        auto* torchBase = GetPlayerTorchBase(player);
        if (!torchBase) {
            return;
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return;
        }

        // Which hand is it in?
        bool inLeft = (player->GetEquippedObject(true) == torchBase);
        bool inRight = (player->GetEquippedObject(false) == torchBase);

        if (!inLeft && !inRight) {
            return;
        }

        // Pick the slot based on hand
        RE::BGSEquipSlot* slot = nullptr;
        if (auto* dom = RE::BGSDefaultObjectManager::GetSingleton()) {
            if (inLeft) {
                slot = dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kLeftHandEquip);
            } else {
                slot = dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kRightHandEquip);
            }
        }

        g_isReequippingTorch = true;

        // NOTE: check your CommonLibSSE NG headers for the exact parameter list of these.
        // This is the usual form; adjust if your version differs.
        equipManager->UnequipObject(player, torchBase,
                                    nullptr,  // extraData
                                    1,        // count
                                    slot,     // equip slot
                                    true,     // queueEquip
                                    false,    // forceEquip
                                    true,     // playSound
                                    false,    // applyNow / locked
                                    nullptr   // unknown / disarm? (depends on version)
        );

        equipManager->EquipObject(player, torchBase,
                                  nullptr,  // extraData
                                  1, slot,
                                  true,    // queueEquip
                                  false,   // forceEquip
                                  true,    // playSound
                                  false);  // applyNow / locked

        g_isReequippingTorch = false;
    }

    // -------------------------------
    // Count Nearby Shadow Lights
    // -------------------------------

    std::uint32_t CountNearbyShadowLights(Actor* center, float radius) {
        auto* console = RE::ConsoleLog::GetSingleton();

        if (!center) {
            if (console) {
                console->Print("[TSL] CountNearbyShadowLights: center actor is null");
            }
            return 0;
        }

        auto* cell = center->GetParentCell();
        if (!cell) {
            if (console) {
                console->Print("[TSL] CountNearbyShadowLights: parent cell is null");
            }
            return 0;
        }

        const auto origin = center->GetPosition();

        std::uint32_t totalLights = 0;
        std::uint32_t shadowLights = 0;

        cell->ForEachReferenceInRange(origin, radius, [&](RE::TESObjectREFR& ref) -> RE::BSContainer::ForEachResult {
            if (ref.IsDeleted() || ref.IsDisabled()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            auto* base = ref.GetBaseObject();
            if (!base) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            if (auto* light = base->As<RE::TESObjectLIGH>()) {
                ++totalLights;

                auto type = GetLightType(light);

                if (type == 1 || type == 3 || type == 5) {
                    ++shadowLights;
                }
            }

            return RE::BSContainer::ForEachResult::kContinue;
        });

        if (console) {
            console->Print("[TSL] Scan radius %.0f: total LIGH=%u, shadow=%u", radius, totalLights, shadowLights);
        }

        return shadowLights;
    }

    // -------------------------------
    // Main Update Logic
    // -------------------------------

    void UpdateTorchShadowState_Native() {
        if (auto* console = ConsoleLog::GetSingleton()) {
            console->Print("Updating torch shadow state...");
        }

        auto* player = PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        // Use a larger radius now
        constexpr float kScanRadius = 6000.0f;

        std::uint32_t shadowCount = CountNearbyShadowLights(player, kScanRadius);
        bool wantShadow = shadowCount < 4;

        if (auto* console = ConsoleLog::GetSingleton()) {
            console->Print("Found %u nearby shadow-casting lights.", shadowCount);
        }

        auto* torchLight = GetPlayerTorchBase(player);
        if (!torchLight) {
            return;
        }

        if (wantShadow != g_lastShadowEnabled) {
            if (wantShadow != g_lastShadowEnabled) {
                SetLightTypeNative(torchLight, wantShadow ? 3u : 2u);
                g_lastShadowEnabled = wantShadow;

                if (auto* console = ConsoleLog::GetSingleton()) {
                    console->Print(wantShadow ? "Torch shadows ENABLED" : "Torch shadows DISABLED");
                }

                ForceReequipTorch(player);
            }
        }
    }

    void StartTorchPollThread() {
        static std::once_flag s_started;

        std::call_once(s_started, [] {
            std::thread([] {
                using namespace std::chrono_literals;

                while (true) {
                    std::this_thread::sleep_for(5s);

                    // Only do work if the torch polling is enabled
                    if (!g_pollTorch.load(std::memory_order_relaxed)) {
                        continue;
                    }

                    auto* taskInterface = SKSE::GetTaskInterface();
                    if (!taskInterface) {
                        continue;
                    }

                    // All game calls must run on the game thread
                    taskInterface->AddTask([]() { UpdateTorchShadowState_Native(); });
                }
            }).detach();
        });
    }

    // Helper: queue onto main thread instead of running in event thread
    void QueueTorchUpdate() {
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([]() {
                // This lambda runs on the main thread
                UpdateTorchShadowState_Native();
            });
        }
    }

    class EquipListener : public BSTEventSink<TESEquipEvent> {
    public:
        static EquipListener* Get() {
            static EquipListener inst;
            return std::addressof(inst);
        }

        static void Install() {
            if (auto* holder = ScriptEventSourceHolder::GetSingleton()) {
                holder->AddEventSink<TESEquipEvent>(Get());
            }
        }

        BSEventNotifyControl ProcessEvent(const TESEquipEvent* a_event, BSTEventSource<TESEquipEvent>*) override {
            if (g_isReequippingTorch) {
                return BSEventNotifyControl::kContinue;
            }

            if (!a_event) {
                return BSEventNotifyControl::kContinue;
            }

            auto* player = PlayerCharacter::GetSingleton();
            if (!player) {
                return BSEventNotifyControl::kContinue;
            }

            // Only care about the player
            if (a_event->actor.get() != player) {
                return BSEventNotifyControl::kContinue;
            }

            // Resolve base form
            auto* form = TESForm::LookupByID(a_event->baseObject);
            if (!form) {
                return BSEventNotifyControl::kContinue;
            }

            auto* asLight = form->As<TESObjectLIGH>();
            if (!asLight) {
                // Not a light/torch, ignore
                return BSEventNotifyControl::kContinue;
            }

            // If equipped: start polling, and do an immediate update
            if (a_event->equipped) {
                g_pollTorch.store(true, std::memory_order_relaxed);
                UpdateTorchShadowState_Native();
            } else {
                // Unequipped that light: stop polling
                g_pollTorch.store(false, std::memory_order_relaxed);
            }

            return BSEventNotifyControl::kContinue;
        }
    };

    // -------------------------------
    // Cell Event Listener (native)
    // -------------------------------

    class CellListener : public BSTEventSink<BGSActorCellEvent>, public BSTEventSink<TESCellFullyLoadedEvent> {
    public:
        static CellListener* Get() {
            static CellListener inst;
            return std::addressof(inst);
        }

        static void Install() {
            auto* player = PlayerCharacter::GetSingleton();
            if (player) {
                if (auto* src = player->AsBGSActorCellEventSource()) {
                    src->AddEventSink<BGSActorCellEvent>(Get());
                }
            }

            if (auto* scriptSrc = ScriptEventSourceHolder::GetSingleton()) {
                scriptSrc->AddEventSink<TESCellFullyLoadedEvent>(Get());
            }
        }

        BSEventNotifyControl ProcessEvent(const BGSActorCellEvent* e, BSTEventSource<BGSActorCellEvent>*) override {
            if (e && e->flags == BGSActorCellEvent::CellFlag::kEnter) {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    // 🚨 IMPORTANT: use AddUITask, NOT AddTask
                    tasks->AddUITask([]() { UpdateTorchShadowState_Native(); });
                }
            }
            return BSEventNotifyControl::kContinue;
        }

        BSEventNotifyControl ProcessEvent(const TESCellFullyLoadedEvent* e,
                                          BSTEventSource<TESCellFullyLoadedEvent>*) override {
            auto* player = PlayerCharacter::GetSingleton();
            if (player && e && e->cell == player->GetParentCell()) {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddUITask([]() { UpdateTorchShadowState_Native(); });
                }
            }
            return BSEventNotifyControl::kContinue;
        }
    };

}

// -------------------------------------
// SKSE entry point
// -------------------------------------

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("Torch Shadow Limiter loaded");
            }

            // Optional: keep cell-based updates
            CellListener::Install();

            // New: equip-based polling & 5s timer
            EquipListener::Install();
            StartTorchPollThread();
        }
    });

    return true;
}
