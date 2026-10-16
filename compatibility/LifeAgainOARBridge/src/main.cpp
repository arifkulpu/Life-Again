#include <SKSE/SKSE.h>
#include <RE/F/FormTraits.h>
#include <RE/B/BGSBipedObjectForm.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <shared_mutex>
#include <mutex>
#include <unordered_map>
#include <string_view>
#include <array>
#include <format>
#include <type_traits>
#include <shlobj_core.h>

#include "LifeAgainAPI.h"
#include "OAR/OpenAnimationReplacerAPI-Conditions.h"
#include "OAR/SKEEPluginInterface.h"

using namespace std::literals;

SKSEPluginInfo(
    .Version = REL::Version{ 1, 0, 0, 0 },
    .Name = "LifeAgainOARBridge"sv,
    .Author = "Arif KULPU"sv,
    .SupportEmail = "support@example.com"sv,
    .StructCompatibility = SKSE::StructCompatibility::Independent,
    .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary,
    .MinimumSKSEVersion = REL::Version{ 2, 0, 0, 0 }
);

namespace {
    using InjuryState = LifeAgain::API::InjuryState;
    std::shared_mutex g_stateMutex;
    std::mutex g_scarMutex;
    std::unordered_map<RE::FormID, InjuryState> g_injuryStates;
    IOverlayInterface* g_overlayInterface = nullptr;
    IOverrideInterface* g_overrideInterface = nullptr;
    std::shared_ptr<spdlog::logger> g_bridgeLogger;
    struct AppliedScar { std::uint32_t slot; std::string texture; };
    std::unordered_map<RE::FormID, std::vector<AppliedScar>> g_appliedScars;
    using InjuryCounts = std::array<std::uint16_t, 8>;
    std::unordered_map<RE::FormID, InjuryCounts> g_injuryCounts;

    constexpr std::array<const char*, 8> kInjuryScarTextures{
        "Actors\\Character\\Character Assets\\Overlays\\Wounds_Smaller_Back.dds",       // abrasion
        "Actors\\Character\\Character Assets\\Overlays\\Wounds_Arm_Left_Small.dds",    // cut
        "Actors\\Character\\Character Assets\\Overlays\\Wounds_Chest.dds",             // minor burn
        "Actors\\Character\\Character Assets\\Overlays\\Wounds_Torso_Bruises.dds",     // bruise
        "Actors\\Character\\Character Assets\\Overlays\\Wounds_Arm_Left_Long.dds",     // deep cut
        "Actors\\Character\\Character Assets\\Overlays\\Wounds_Arm_Right.dds",         // puncture
        "Actors\\Character\\Character Assets\\Overlays\\Wounds_Legs_Left_Bruises.dds", // crush
        "Actors\\Character\\Character Assets\\Overlays\\Wounds_Legs_Right.dds"         // severe burn
    };
    // RaceMenu overlay color values use opaque ARGB. Keep the tint modest so
    // the wound texture remains visible while each injury reads distinctly.
    constexpr std::array<skee_i32, 8> kInjuryScarColors{
        static_cast<skee_i32>(0xFFFFC4A8), // abrasion: irritated pink
        static_cast<skee_i32>(0xFFFF8A78), // cut: fresh red
        static_cast<skee_i32>(0xFFFF9A68), // minor burn: warm red-orange
        static_cast<skee_i32>(0xFF8A6688), // bruise: muted purple
        static_cast<skee_i32>(0xFFCC5148), // deep cut: dark red
        static_cast<skee_i32>(0xFFB9463E), // puncture: deep crimson
        static_cast<skee_i32>(0xFF765D70), // crush: dark bruised purple
        static_cast<skee_i32>(0xFF9B4036)  // severe burn: dark burnt red
    };

    template <class T>
    class TypedValue final : public IOverrideInterface::SetVariant {
    public:
        explicit TypedValue(T a_value) : value(a_value) {}
        Type GetType() override;
        skee_i32 Int() override { if constexpr (std::is_same_v<T, skee_i32>) return value; else return 0; }
        float Float() override { if constexpr (std::is_same_v<T, float>) return value; else return 0.0f; }
        const char* String() override { if constexpr (std::is_same_v<T, const char*>) return value; else return nullptr; }
    private:
        T value;
    };

    template <> IOverrideInterface::SetVariant::Type TypedValue<skee_i32>::GetType() { return Type::Int; }
    template <> IOverrideInterface::SetVariant::Type TypedValue<float>::GetType() { return Type::Float; }
    template <> IOverrideInterface::SetVariant::Type TypedValue<const char*>::GetType() { return Type::String; }

    class StringValue final : public IOverrideInterface::GetVariant {
    public:
        void Int(skee_i32) override {}
        void Float(float) override {}
        void String(const char* a_value) override { value = a_value; }
        void Bool(bool) override {}
        void TextureSet(const RE::BGSTextureSet*) override {}
        const char* value = nullptr;
    };

    bool IsFreeOverlaySlot(RE::Actor* a_actor, bool a_isFemale, const char* a_node)
    {
        if (!g_overrideInterface->HasNodeOverride(a_actor, a_isFemale, a_node, 9, 0)) return true;
        StringValue texture;
        if (!g_overrideInterface->GetNodeOverride(a_actor, a_isFemale, a_node, 9, 0, texture)) return true;
        if (!texture.value || !*texture.value) return true;
        std::string path(texture.value);
        std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return path.find("default.dds") != std::string::npos;
    }

    bool IsFemale(RE::Actor* a_actor)
    {
        const auto* base = a_actor ? a_actor->GetActorBase() : nullptr;
        return base && base->GetSex() == RE::SEX::kFemale;
    }

    void RevertScarNode(RE::Actor* a_actor, const AppliedScar& a_scar)
    {
        if (!a_actor || !g_overrideInterface) return;
        const auto node = std::format("Body [ovl{}]", a_scar.slot);
        if (g_overlayInterface) {
            constexpr auto bodyMask = static_cast<skee_u32>(RE::BIPED_MODEL::BipedObjectSlot::kBody);
            spdlog::info("LifeAgainOARBridge: reverting RaceMenu body overlay node {} on {:08X}.", node, a_actor->GetFormID());
            // Revert the specific RaceMenu overlay geometry; RemoveNodeOverride alone
            // only clears the saved property override and can leave the rendered decal.
            g_overlayInterface->RevertOverlay(a_actor, node.c_str(), bodyMask, bodyMask, true, true);
            spdlog::info("LifeAgainOARBridge: RaceMenu RevertOverlay returned for {:08X}, slot {}.", a_actor->GetFormID(), a_scar.slot);
        }
        for (const auto [key, index] : { std::pair<std::uint16_t, std::uint8_t>{9, 0}, {7, 255}, {0, 255}, {1, 255}, {8, 255}, {2, 255}, {3, 255} }) {
            g_overrideInterface->RemoveNodeOverride(a_actor, IsFemale(a_actor), node.c_str(), key, index);
        }
    }

    void RemoveScar(RE::FormID a_formID)
    {
        spdlog::info("LifeAgainOARBridge: scar removal started for {:08X}.", a_formID);
        std::scoped_lock lock(g_scarMutex);
        const auto it = g_appliedScars.find(a_formID);
        if (it == g_appliedScars.end()) {
            spdlog::info("LifeAgainOARBridge: no applied scars to remove for {:08X}.", a_formID);
            return;
        }

        auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_formID);
        if (actor && g_overrideInterface) {
            for (const auto& scar : it->second) {
                RevertScarNode(actor, scar);
            }
            spdlog::info("LifeAgainOARBridge: updating RaceMenu node properties after scar removal for {:08X}.", a_formID);
            g_overrideInterface->SetNodeProperties(actor, true);
        }
        g_appliedScars.erase(it);
        spdlog::info("LifeAgainOARBridge: scar removal completed for {:08X}.", a_formID);
    }

    void ApplyScars(RE::FormID a_formID, const InjuryCounts& a_counts)
    {
        std::scoped_lock lock(g_scarMutex);
        if (!g_overlayInterface || !g_overrideInterface) {
            spdlog::warn("LifeAgainOARBridge: scar application skipped for {:08X}; RaceMenu interfaces unavailable.", a_formID);
            return;
        }
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_formID);
        if (!actor) {
            spdlog::warn("LifeAgainOARBridge: scar application skipped; actor {:08X} was not found.", a_formID);
            return;
        }

        spdlog::info("LifeAgainOARBridge: scar application started for {:08X}; counts={},{},{},{},{},{},{},{}.",
            a_formID, a_counts[0], a_counts[1], a_counts[2], a_counts[3], a_counts[4], a_counts[5], a_counts[6], a_counts[7]);

        if (const auto old = g_appliedScars.find(a_formID); old != g_appliedScars.end()) {
            for (const auto& scar : old->second) {
                RevertScarNode(actor, scar);
            }
            g_appliedScars.erase(old);
        }

        spdlog::info("LifeAgainOARBridge: calling RaceMenu AddOverlays for {:08X}.", a_formID);
        g_overlayInterface->AddOverlays(actor, false);
        spdlog::info("LifeAgainOARBridge: AddOverlays returned for {:08X}.", a_formID);
        const auto slotCount = g_overlayInterface->GetOverlayCount(IOverlayInterface::OverlayType::Normal, IOverlayInterface::OverlayLocation::Body);
        spdlog::info("LifeAgainOARBridge: body overlay slots available for {:08X}: {}.", a_formID, slotCount);
        auto& applied = g_appliedScars[a_formID];
        for (std::size_t type = 0; type < a_counts.size(); ++type) {
            if (!a_counts[type]) continue;
            std::uint32_t slot = slotCount;
            for (std::uint32_t candidate = 0; candidate < slotCount; ++candidate) {
                const auto node = std::format("Body [ovl{}]", candidate);
                if (IsFreeOverlaySlot(actor, IsFemale(actor), node.c_str())) { slot = candidate; break; }
            }
            if (slot == slotCount) {
                spdlog::warn("LifeAgainOARBridge: no free RaceMenu body overlay slot for {:08X} (configured slots: {})", a_formID, slotCount);
                break;
            }
            const auto texture = kInjuryScarTextures[type];
            const auto node = std::format("Body [ovl{}]", slot);
            TypedValue<const char*> textureValue(texture);
            TypedValue<skee_i32> colorValue(kInjuryScarColors[type]);
            TypedValue<skee_i32> neutralValue(0);
            TypedValue<float> opacityValue(1.0f);
            TypedValue<float> zeroValue(0.0f);
            g_overrideInterface->AddNodeOverride(actor, IsFemale(actor), node.c_str(), 9, 0, textureValue);
            g_overrideInterface->AddNodeOverride(actor, IsFemale(actor), node.c_str(), 7, 255, colorValue);
            g_overrideInterface->AddNodeOverride(actor, IsFemale(actor), node.c_str(), 0, 255, neutralValue);
            g_overrideInterface->AddNodeOverride(actor, IsFemale(actor), node.c_str(), 1, 255, opacityValue);
            g_overrideInterface->AddNodeOverride(actor, IsFemale(actor), node.c_str(), 8, 255, opacityValue);
            g_overrideInterface->AddNodeOverride(actor, IsFemale(actor), node.c_str(), 2, 255, zeroValue);
            g_overrideInterface->AddNodeOverride(actor, IsFemale(actor), node.c_str(), 3, 255, zeroValue);
            applied.push_back(AppliedScar{ slot, texture });
            spdlog::info("LifeAgainOARBridge: wound type {} scar applied to {:08X}, slot {}, texture {}", type, a_formID, slot, texture);
        }
        const auto appliedCount = applied.size();
        if (applied.empty()) g_appliedScars.erase(a_formID);
        spdlog::info("LifeAgainOARBridge: calling SetNodeProperties for {:08X}; appliedScars={}", a_formID, appliedCount);
        g_overrideInterface->SetNodeProperties(actor, true);
        spdlog::info("LifeAgainOARBridge: SetNodeProperties returned for {:08X}.", a_formID);
    }

    void QueueScarUpdate(RE::FormID a_formID, InjuryState a_state, InjuryCounts a_counts = {})
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            spdlog::error("LifeAgainOARBridge: cannot queue scar update for {:08X}; task interface unavailable.", a_formID);
            return;
        }
        spdlog::info("LifeAgainOARBridge: queueing scar update for {:08X}; state={}; counts={},{},{},{},{},{},{},{}.",
            a_formID, static_cast<int>(a_state), a_counts[0], a_counts[1], a_counts[2], a_counts[3], a_counts[4], a_counts[5], a_counts[6], a_counts[7]);
        tasks->AddTask([a_formID, a_state, a_counts]() {
            spdlog::info("LifeAgainOARBridge: scar update task started for {:08X}; state={}.", a_formID, static_cast<int>(a_state));
            if (a_state == InjuryState::Cleared) RemoveScar(a_formID);
            else ApplyScars(a_formID, a_counts);
            spdlog::info("LifeAgainOARBridge: scar update task completed for {:08X}.", a_formID);
        });
    }

    InjuryState GetState(RE::FormID a_formID)
    {
        std::shared_lock lock(g_stateMutex);
        const auto it = g_injuryStates.find(a_formID);
        return it == g_injuryStates.end() ? InjuryState::Cleared : it->second;
    }

    bool HasInjuryType(RE::FormID a_formID, std::initializer_list<std::size_t> a_types)
    {
        std::shared_lock lock(g_stateMutex);
        const auto it = g_injuryCounts.find(a_formID);
        if (it == g_injuryCounts.end()) return false;
        return std::ranges::any_of(a_types, [&](std::size_t type) {
            return type < it->second.size() && it->second[type] > 0;
        });
    }

    void HandleLifeAgainEvent(const SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message || a_message->dataLen != sizeof(LifeAgain::API::FollowerInjuryStateEvent) || !a_message->data) {
            return;
        }

        const auto& event = *static_cast<const LifeAgain::API::FollowerInjuryStateEvent*>(a_message->data);
        if (event.apiVersion != LifeAgain::API::kVersion) {
            spdlog::warn("LifeAgainOARBridge: Unsupported Life Again API version {}", event.apiVersion);
            return;
        }

        spdlog::info("LifeAgainOARBridge: injury event {:08X}, state {}, type {}, count {}",
            event.formID, static_cast<int>(event.state), static_cast<int>(event.injuryType), event.injuryCount);

        std::unique_lock lock(g_stateMutex);
        if (event.formID == 0 && event.state == InjuryState::Cleared) {
            std::vector<RE::FormID> appliedFormIDs;
            {
                std::scoped_lock scarLock(g_scarMutex);
                appliedFormIDs.reserve(g_appliedScars.size());
                for (const auto& [formID, _] : g_appliedScars) appliedFormIDs.push_back(formID);
            }
            for (const auto formID : appliedFormIDs) QueueScarUpdate(formID, InjuryState::Cleared);
            g_injuryStates.clear();
            g_injuryCounts.clear();
        } else if (event.state == InjuryState::Cleared) {
            g_injuryStates.erase(event.formID);
            g_injuryCounts.erase(event.formID);
            QueueScarUpdate(event.formID, InjuryState::Cleared);
        } else {
            g_injuryStates[event.formID] = event.state;
            auto& counts = g_injuryCounts[event.formID];
            if (event.injuryType < LifeAgain::API::InjuryType::None) {
                const auto index = static_cast<std::size_t>(event.injuryType);
                if (index < counts.size()) counts[index] = event.injuryCount;
            }
            QueueScarUpdate(event.formID, event.state, counts);
        }
    }

    class IsLifeAgainInjured : public Conditions::CustomCondition {
    public:
        constexpr static inline std::string_view CONDITION_NAME = "LifeAgainIsInjured"sv;
        RE::BSString GetName() const override { return CONDITION_NAME.data(); }
        RE::BSString GetDescription() const override { return "True when this actor has a Life Again injury."; }
        constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*) const override
        {
            auto* actor = a_refr ? a_refr->As<RE::Actor>() : nullptr;
            if (!actor) return false;
            return GetState(actor->GetFormID()) != InjuryState::Cleared;
        }
    };

    class IsLifeAgainHeavilyInjured : public Conditions::CustomCondition {
    public:
        constexpr static inline std::string_view CONDITION_NAME = "LifeAgainIsHeavilyInjured"sv;
        RE::BSString GetName() const override { return CONDITION_NAME.data(); }
        RE::BSString GetDescription() const override { return "True when this actor has a heavy Life Again injury."; }
        constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*) const override
        {
            auto* actor = a_refr ? a_refr->As<RE::Actor>() : nullptr;
            if (!actor) return false;
            return GetState(actor->GetFormID()) == InjuryState::Heavy;
        }
    };

    class IsLifeAgainLightlyInjured : public Conditions::CustomCondition {
    public:
        constexpr static inline std::string_view CONDITION_NAME = "LifeAgainIsLightlyInjured"sv;
        RE::BSString GetName() const override { return CONDITION_NAME.data(); }
        RE::BSString GetDescription() const override { return "True when this actor has light Life Again injuries and no heavy injuries."; }
        constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*) const override
        {
            auto* actor = a_refr ? a_refr->As<RE::Actor>() : nullptr;
            if (!actor) return false;
            return GetState(actor->GetFormID()) == InjuryState::Light;
        }
    };

    class HasLifeAgainArmInjury : public Conditions::CustomCondition {
    public:
        constexpr static inline std::string_view CONDITION_NAME = "LifeAgainHasArmInjury"sv;
        RE::BSString GetName() const override { return CONDITION_NAME.data(); }
        RE::BSString GetDescription() const override { return "True for Life Again deep cuts or punctures, mapped to wounded-arm animations."; }
        constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*) const override
        {
            auto* actor = a_refr ? a_refr->As<RE::Actor>() : nullptr;
            return actor && HasInjuryType(actor->GetFormID(), { 4, 5 });
        }
    };

    class HasLifeAgainLegInjury : public Conditions::CustomCondition {
    public:
        constexpr static inline std::string_view CONDITION_NAME = "LifeAgainHasLegInjury"sv;
        RE::BSString GetName() const override { return CONDITION_NAME.data(); }
        RE::BSString GetDescription() const override { return "True for Life Again crush injuries, mapped to wounded-leg animations."; }
        constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*) const override
        {
            auto* actor = a_refr ? a_refr->As<RE::Actor>() : nullptr;
            return actor && HasInjuryType(actor->GetFormID(), { 6 });
        }
    };

    template <class T>
    void RegisterOARCondition()
    {
        const auto result = OAR_API::Conditions::AddCustomCondition<T>();
        if (result == OAR_API::Conditions::APIResult::OK || result == OAR_API::Conditions::APIResult::AlreadyRegistered) {
            spdlog::info("LifeAgainOARBridge: OAR condition ready: {}", T::CONDITION_NAME);
        } else {
            spdlog::warn("LifeAgainOARBridge: Could not register OAR condition {} ({})", T::CONDITION_NAME, static_cast<int>(result));
        }
    }

    void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message) return;
        if (a_message->type == LifeAgain::API::kFollowerInjuryStateMessage) {
            HandleLifeAgainEvent(a_message);
        } else if (a_message->type == SKSE::MessagingInterface::kPostLoad) {
            if (!OAR_API::Conditions::GetAPI()) {
                spdlog::info("LifeAgainOARBridge: OAR not installed; visual conditions are unavailable.");
                return;
            }
            RegisterOARCondition<IsLifeAgainInjured>();
            RegisterOARCondition<IsLifeAgainLightlyInjured>();
            RegisterOARCondition<IsLifeAgainHeavilyInjured>();
            RegisterOARCondition<HasLifeAgainArmInjury>();
            RegisterOARCondition<HasLifeAgainLegInjury>();
        } else if (a_message->type == SKSE::MessagingInterface::kPostPostLoad) {
            InterfaceExchangeMessage exchange{};
            if (auto* messaging = SKSE::GetMessagingInterface()) {
                messaging->Dispatch(InterfaceExchangeMessage::kMessage_ExchangeInterface, &exchange, sizeof(exchange), "skee");
            }
            if (exchange.interfaceMap) {
                g_overlayInterface = static_cast<IOverlayInterface*>(exchange.interfaceMap->QueryInterface("Overlay"));
                g_overrideInterface = static_cast<IOverrideInterface*>(exchange.interfaceMap->QueryInterface("Override"));
            }
            spdlog::info("LifeAgainOARBridge: RaceMenu overlay API {}", g_overlayInterface && g_overrideInterface ? "ready" : "unavailable");
            if (g_overlayInterface && g_overrideInterface) {
                std::shared_lock lock(g_stateMutex);
                for (const auto& [formID, state] : g_injuryStates) {
                    const auto counts = g_injuryCounts.find(formID);
                    QueueScarUpdate(formID, state, counts == g_injuryCounts.end() ? InjuryCounts{} : counts->second);
                }
            }
        }
    }
}

void InitializeBridgeLogging()
{
    try {
        std::filesystem::path logDirectory;
        PWSTR documentsPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documentsPath))) {
            logDirectory = std::filesystem::path(documentsPath) / "My Games" / "Skyrim Special Edition" / "SKSE";
            CoTaskMemFree(documentsPath);
        } else {
            const auto fallback = SKSE::log::log_directory();
            if (!fallback) return;
            logDirectory = *fallback;
        }
        std::filesystem::create_directories(logDirectory);
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((logDirectory / "LifeAgainOARBridge.log").string(), true);
        g_bridgeLogger = std::make_shared<spdlog::logger>("LifeAgainOARBridge", sink);
        g_bridgeLogger->set_level(spdlog::level::debug);
        g_bridgeLogger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(g_bridgeLogger);
        spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    } catch (const std::exception&) {
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    InitializeBridgeLogging();
    spdlog::info("LifeAgainOARBridge: loaded");
    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener("SKSE", OnSKSEMessage) ||
        !messaging->RegisterListener("LifeAgain", OnSKSEMessage)) {
        spdlog::error("LifeAgainOARBridge: failed to register SKSE/LifeAgain message listeners");
        return false;
    }
    spdlog::info("LifeAgainOARBridge: listening for SKSE lifecycle and LifeAgain injury events");
    return true;
}
