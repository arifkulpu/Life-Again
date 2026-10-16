#pragma once
#include <SKSE/SKSE.h>
#undef PlaySound
#include <random>
#include <array>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <mutex>
#include <chrono>
#include <ctime>
#include <numeric>
#include <atomic>
#include <optional>
#include <functional>
#include <thread>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <spdlog/spdlog.h>
#include <RE/S/ScriptEventSourceHolder.h>
#include <RE/C/Calendar.h>
#include <RE/T/TESRace.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/ProcessLists.h>
#include <RE/T/TESDeathEvent.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESActivateEvent.h>
#include <RE/T/TESEnterBleedoutEvent.h>
#include <RE/T/TESContainerChangedEvent.h>
#include <RE/T/TESHitEvent.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/U/UI.h>
#include <RE/M/Misc.h>
#include <RE/B/BGSLocation.h>
#include <RE/B/BGSKeyword.h>
#include <RE/I/InventoryChanges.h>
#include <RE/A/AlchemyItem.h>
#include <RE/T/TESDataHandler.h>
#include "LifeAgainAPI.h"

namespace LifeAgain {

    // Yoldas kategorisi
    enum class FollowerKind : int {
        Human  = 0,
        Dog    = 1,
        Cat    = 2,
        Horse  = 3,
        Other  = 4
    };

    // Ölen bir yoldası temsil eden yapı
    struct DeadFollower {
        RE::FormID  formID   = 0;
        std::string name;
        std::string deathDate;  // "2026-08-06 17:45"
        int         level    = 1;
        FollowerKind kind    = FollowerKind::Human;
    };

    enum class InjuryLevel : int {
        None = 0,
        Light = 1,
        Heavy = 2
    };

    enum class InjuryType : std::uint8_t {
        Abrasion = 0,
        Cut = 1,
        MinorBurn = 2,
        Bruise = 3,
        DeepCut = 4,
        Puncture = 5,
        Crush = 6,
        SevereBurn = 7,
        Count = 8
    };

    inline constexpr std::size_t kInjuryTypeCount = static_cast<std::size_t>(InjuryType::Count);
    inline constexpr std::array<const char*, kInjuryTypeCount> kInjuryTypeNames{
        "Abrasion (Siyrik)", "Cut (Kesik)", "Minor Burn (Hafif Yanık)", "Bruise (Ezik/Morluk)",
        "Deep Cut (Derin Kesik)", "Puncture (Delici Yara)", "Crush (Ezilme/Parçalanma)", "Severe Burn (Ciddi Yanık)"
    };

    inline InjuryLevel GetInjuryLevel(const std::array<std::uint16_t, kInjuryTypeCount>& a_counts) {
        for (std::size_t i = 4; i < a_counts.size(); ++i) {
            if (a_counts[i] > 0) return InjuryLevel::Heavy;
        }
        for (std::size_t i = 0; i < 4; ++i) {
            if (a_counts[i] > 0) return InjuryLevel::Light;
        }
        return InjuryLevel::None;
    }

    inline int GetInjuryPotionCost(const std::array<std::uint16_t, kInjuryTypeCount>& a_counts) {
        int cost = 0;
        for (std::size_t i = 0; i < a_counts.size(); ++i) {
            cost += static_cast<int>(a_counts[i]) * (i < 4 ? 1 : 3);
        }
        return cost;
    }

    inline int GetInjuryGoldCost(const std::array<std::uint16_t, kInjuryTypeCount>& a_counts) {
        int cost = 0;
        for (std::size_t i = 0; i < a_counts.size(); ++i) {
            cost += static_cast<int>(a_counts[i]) * (i < 4 ? 100 : 250);
        }
        return cost;
    }

    inline void DispatchFollowerInjuryState(RE::FormID a_formID, InjuryLevel a_level, InjuryType a_type = InjuryType::Count, std::uint16_t a_count = 0);
    inline void PublishFollowerInjuryState(RE::FormID a_formID, InjuryLevel a_level, InjuryType a_type = InjuryType::Count, std::uint16_t a_count = 0);
    struct InjuredFollower;
    inline void CheckPotionHealing(RE::Actor* actor, InjuredFollower* inf);
    inline bool IsPriest(RE::Actor* actor);

    struct InjuredFollower {
        RE::FormID formID = 0;
        std::string name;
        int level = 1;
        FollowerKind kind = FollowerKind::Human;
        InjuryLevel injuryLevel = InjuryLevel::Light;
        std::array<std::uint16_t, kInjuryTypeCount> injuryCounts{};
        float savedSpeedBase = 100.0f; // Yaralanmadan onceki baz SpeedMult degeri (iyilesince geri yuklenir)
        float recoveryLastGameDay = -1.0f;
        float recoveryProgressDays = 0.0f;
    };

    inline void PublishFollowerInjuryRecord(const InjuredFollower& a_follower);


    // ---- Rahip erişim kontrolü (global flag) ----
    // true = oyuncu tapınaktaki bir rahiple konuştu, menüyü açabilir
    inline std::atomic<bool> g_PriestAccess{ false };

    // Son etkileşilen rahip NPC'nin adı (menüde göstermek için)
    inline std::string g_PriestName;

    // -- Singleton veri deposu --
    class FollowerTracker {
    public:
        static FollowerTracker& GetSingleton() {
            static FollowerTracker instance;
            return instance;
        }

        // Tüm ölü yoldaşlar (thread-safe)
        std::vector<DeadFollower> GetDeadFollowers() {
            std::lock_guard lock(m_mutex);
            return m_dead;
        }

        void AddDead(DeadFollower df) {
            std::lock_guard lock(m_mutex);
            for (auto& d : m_dead)
                if (d.formID == df.formID) return;
            m_dead.push_back(std::move(df));
        }

        void Remove(RE::FormID id) {
            std::lock_guard lock(m_mutex);
            std::erase_if(m_dead, [id](const DeadFollower& d) { return d.formID == id; });
        }

        std::vector<InjuredFollower> GetInjuredFollowers() {
            std::lock_guard lock(m_mutex);
            return m_injured;
        }

        void AddOrUpdateInjured(InjuredFollower inv) {
            const auto formID = inv.formID;
            {
                std::lock_guard lock(m_mutex);
                bool found = false;
                for (auto& i : m_injured) {
                    if (i.formID == inv.formID) {
                        i.injuryLevel = inv.injuryLevel;
                        i.injuryCounts = inv.injuryCounts;
                        i.recoveryLastGameDay = inv.recoveryLastGameDay;
                        i.recoveryProgressDays = inv.recoveryProgressDays;
                        found = true;
                        break;
                    }
                }
                if (!found) m_injured.push_back(std::move(inv));
            }
            const auto current = GetInjured(formID);
            if (current) PublishFollowerInjuryRecord(*current);
        }

        void RemoveInjured(RE::FormID id) {
            bool removed = false;
            {
                std::lock_guard lock(m_mutex);
                const auto oldSize = m_injured.size();
                std::erase_if(m_injured, [id](const InjuredFollower& i) { return i.formID == id; });
                removed = oldSize != m_injured.size();
            }
            if (removed) PublishFollowerInjuryState(id, InjuryLevel::None);
        }

        // Kopya döndürür: döndürülen değer vektör realokasyonunda sarkmaz
        std::optional<InjuredFollower> GetInjured(RE::FormID id) {
            std::lock_guard lock(m_mutex);
            for (auto& i : m_injured) {
                if (i.formID == id) return i;
            }
            return std::nullopt;
        }

        // Tek bir yaralı kaydını mutex kilidi altında günceller
        void UpdateInjured(RE::FormID id, const std::function<void(InjuredFollower&)>& a_fn) {
            std::lock_guard lock(m_mutex);
            for (auto& i : m_injured) {
                if (i.formID == id) {
                    a_fn(i);
                    return;
                }
            }
        }

        int GetReviveCount(RE::FormID id) {
            std::lock_guard lock(m_mutex);
            auto it = m_reviveCounts.find(id);
            if (it != m_reviveCounts.end()) {
                return it->second;
            }
            return 0;
        }

        void AddReviveCount(RE::FormID id) {
            std::lock_guard lock(m_mutex);
            m_reviveCounts[id]++;
        }

        void Clear() {
            {
                std::lock_guard lock(m_mutex);
                m_dead.clear();
                m_injured.clear();
                m_reviveCounts.clear();
            }
            PublishFollowerInjuryState(0, InjuryLevel::None);
            spdlog::info("FollowerTracker: Tum veriler temizlendi (Revert).");
        }

        std::string SerializeToJsonString() {
            std::lock_guard lock(m_mutex);
            nlohmann::json j;
            
            nlohmann::json deadArr = nlohmann::json::array();
            for (auto& df : m_dead) {
                nlohmann::json item;
                item["formID"]    = df.formID;
                item["name"]      = df.name;
                item["deathDate"] = df.deathDate;
                item["level"]     = df.level;
                item["kind"]      = static_cast<int>(df.kind);
                deadArr.push_back(item);
            }
            j["dead"] = deadArr;

            nlohmann::json injuredArr = nlohmann::json::array();
            for (auto& inf : m_injured) {
                nlohmann::json item;
                item["formID"] = inf.formID;
                item["name"] = inf.name;
                item["level"] = inf.level;
                item["kind"] = static_cast<int>(inf.kind);
                item["injuryLevel"] = static_cast<int>(inf.injuryLevel);
                item["injuryCounts"] = inf.injuryCounts;
                item["savedSpeedBase"] = inf.savedSpeedBase;
                item["recoveryLastGameDay"] = inf.recoveryLastGameDay;
                item["recoveryProgressDays"] = inf.recoveryProgressDays;
                injuredArr.push_back(item);
            }
            j["injured"] = injuredArr;


            nlohmann::json revivesObj = nlohmann::json::object();
            for (const auto& [id, count] : m_reviveCounts) {
                revivesObj[std::to_string(id)] = count;
            }
            j["reviveCounts"] = revivesObj;

            return j.dump();
        }

        void DeserializeFromJsonString(const std::string& str, const SKSE::SerializationInterface* a_intfc = nullptr) {
            if (str.empty()) return;
            try {
                std::lock_guard lock(m_mutex);
                auto j = nlohmann::json::parse(str);
                
                m_dead.clear();
                if (j.contains("dead")) {
                    for (auto& item : j["dead"]) {
                        DeadFollower df;
                        RE::FormID rawID = item.value("formID", 0u);
                        RE::FormID resolvedID = rawID;
                        if (a_intfc && rawID != 0) {
                            a_intfc->ResolveFormID(rawID, resolvedID);
                        }
                        df.formID    = resolvedID;
                        df.name      = item.value("name", std::string("Unknown"));
                        df.deathDate = item.value("deathDate", std::string("Unknown"));
                        df.level     = item.value("level", 1);
                        df.kind      = static_cast<FollowerKind>(item.value("kind", 0));
                        m_dead.push_back(df);
                    }
                }

                m_injured.clear();
                if (j.contains("injured")) {
                    for (auto& item : j["injured"]) {
                        InjuredFollower inf;
                        RE::FormID rawID = item.value("formID", 0u);
                        RE::FormID resolvedID = rawID;
                        if (a_intfc && rawID != 0) {
                            a_intfc->ResolveFormID(rawID, resolvedID);
                        }
                        inf.formID = resolvedID;
                        inf.name = item.value("name", std::string("Unknown"));
                        inf.level = item.value("level", 1);
                        inf.kind = static_cast<FollowerKind>(item.value("kind", 0));
                        inf.injuryLevel = static_cast<InjuryLevel>(item.value("injuryLevel", 1));
                        if (item.contains("injuryCounts") && item["injuryCounts"].is_array()) {
                            const auto& counts = item["injuryCounts"];
                            for (std::size_t n = 0; n < std::min(counts.size(), kInjuryTypeCount); ++n) {
                                inf.injuryCounts[n] = counts[n].get<std::uint16_t>();
                            }
                        } else {
                            // Migrate an old save's single Light/Heavy status to the new wound list.
                            inf.injuryCounts[inf.injuryLevel == InjuryLevel::Heavy ? 4 : 1] = 1;
                        }
                        inf.injuryLevel = GetInjuryLevel(inf.injuryCounts);
                        inf.savedSpeedBase = item.value("savedSpeedBase", 100.0f);
                        inf.recoveryLastGameDay = item.value("recoveryLastGameDay", -1.0f);
                        inf.recoveryProgressDays = item.value("recoveryProgressDays", 0.0f);
                        m_injured.push_back(inf);
                    }
                }

                m_reviveCounts.clear();
                if (j.contains("reviveCounts")) {
                    for (auto& [key, value] : j["reviveCounts"].items()) {
                        RE::FormID rawID = std::stoul(key);
                        RE::FormID resolvedID = rawID;
                        if (a_intfc && rawID != 0) {
                            a_intfc->ResolveFormID(rawID, resolvedID);
                        }
                        m_reviveCounts[resolvedID] = value.get<int>();
                    }
                }
                spdlog::info("FollowerTracker: Save dosyasindan {} olu, {} yarali yoldas yuklendi.", m_dead.size(), m_injured.size());
            } catch (const std::exception& e) {
                spdlog::error("FollowerTracker: JSON deserialize error: {}", e.what());
            }
        }

    private:
        std::mutex m_mutex;
        std::vector<DeadFollower> m_dead;
        std::vector<InjuredFollower> m_injured;
        std::map<RE::FormID, int> m_reviveCounts;
    };

    inline void DispatchFollowerInjuryState(RE::FormID a_formID, InjuryLevel a_level, InjuryType a_type, std::uint16_t a_count) {
        auto* messaging = SKSE::GetMessagingInterface();
        auto trace = spdlog::get("LifeAgainTreatment");
        if (!messaging) {
            if (trace) trace->error("Injury visual dispatch skipped: SKSE messaging interface unavailable; actor={:08X}.", a_formID);
            return;
        }

        API::FollowerInjuryStateEvent event{};
        event.formID = a_formID;
        switch (a_level) {
            case InjuryLevel::Light: event.state = API::InjuryState::Light; break;
            case InjuryLevel::Heavy: event.state = API::InjuryState::Heavy; break;
            default: event.state = API::InjuryState::Cleared; break;
        }
        event.injuryType = a_type == InjuryType::Count ? API::InjuryType::None : static_cast<API::InjuryType>(a_type);
        event.injuryCount = a_count;
        if (trace) trace->info("Dispatching injury visual state: actor={:08X}, state={}, type={}, count={}",
            a_formID, static_cast<int>(event.state), static_cast<int>(event.injuryType), event.injuryCount);
        messaging->Dispatch(API::kFollowerInjuryStateMessage, &event, sizeof(event), nullptr);
        if (trace) trace->info("Injury visual state dispatch returned: actor={:08X}.", a_formID);
    }

    inline void PublishFollowerInjuryState(RE::FormID a_formID, InjuryLevel a_level, InjuryType a_type, std::uint16_t a_count) {
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([a_formID, a_level, a_type, a_count]() {
                DispatchFollowerInjuryState(a_formID, a_level, a_type, a_count);
            });
        } else {
            DispatchFollowerInjuryState(a_formID, a_level, a_type, a_count);
        }
    }

    inline void PublishFollowerInjuryRecord(const InjuredFollower& a_follower) {
        PublishFollowerInjuryState(a_follower.formID, InjuryLevel::None);
        for (std::size_t i = 0; i < a_follower.injuryCounts.size(); ++i) {
            const auto count = a_follower.injuryCounts[i];
            if (count == 0) continue;
            const auto type = static_cast<InjuryType>(i);
            PublishFollowerInjuryState(a_follower.formID, i < 4 ? InjuryLevel::Light : InjuryLevel::Heavy, type, count);
        }
    }

    inline void PublishFollowerInjurySnapshot() {
        DispatchFollowerInjuryState(0, InjuryLevel::None);
        const auto injured = FollowerTracker::GetSingleton().GetInjuredFollowers();
        for (const auto& follower : injured) {
            PublishFollowerInjuryRecord(follower);
        }
    }


    // ---- Yardımcı fonksiyonlar ----

    inline void RemoveInjuryDebuffs(RE::Actor* actor, InjuryLevel level) {
        if (!actor || level == InjuryLevel::None) return;
        auto owner = actor->AsActorValueOwner();
        if (owner) {
            float damageMod = (level == InjuryLevel::Heavy) ? 0.25f : 0.10f;
            float speedMod  = (level == InjuryLevel::Heavy) ? 45.0f : 10.0f;
            float regenMod  = (level == InjuryLevel::Heavy) ? 25.0f : 15.0f;
            float armorMod  = (level == InjuryLevel::Heavy) ? 60.0f : 30.0f;

            owner->ModActorValue(RE::ActorValue::kAttackDamageMult, damageMod);
            owner->ModActorValue(RE::ActorValue::kSpeedMult, speedMod);
            owner->ModActorValue(RE::ActorValue::kHealRateMult, regenMod);
            owner->ModActorValue(RE::ActorValue::kMagickaRateMult, regenMod);
            owner->ModActorValue(RE::ActorValue::kStaminaRateMult, regenMod);
            owner->ModActorValue(RE::ActorValue::kDamageResist, armorMod);
        }
        // Yaralı animasyon durumunu sıfırla
        actor->NotifyAnimationGraph("IdleWoundedStop");
        spdlog::info("LifeAgain: Injury debuffs removed from {}", actor->GetDisplayFullName());
    }

    // savedSpeedBase kullanan overload: hizi kesin olarak yaralanma oncesi degere geri yukler
    inline void RemoveInjuryDebuffs(RE::Actor* actor, InjuredFollower* inf) {
        if (!actor || !inf || inf->injuryLevel == InjuryLevel::None) return;
        auto owner = actor->AsActorValueOwner();
        if (owner) {
            float damageMod = (inf->injuryLevel == InjuryLevel::Heavy) ? 0.25f : 0.10f;
            float regenMod  = (inf->injuryLevel == InjuryLevel::Heavy) ? 25.0f : 15.0f;
            float armorMod  = (inf->injuryLevel == InjuryLevel::Heavy) ? 60.0f : 30.0f;

            owner->ModActorValue(RE::ActorValue::kAttackDamageMult, damageMod);
            owner->ModActorValue(RE::ActorValue::kHealRateMult, regenMod);
            owner->ModActorValue(RE::ActorValue::kMagickaRateMult, regenMod);
            owner->ModActorValue(RE::ActorValue::kStaminaRateMult, regenMod);
            owner->ModActorValue(RE::ActorValue::kDamageResist, armorMod);

            // Hizi kesin olarak eski baz degere sıfırla (stacking sorununu temizler)
            float currentBase = owner->GetBaseActorValue(RE::ActorValue::kSpeedMult);
            float delta = inf->savedSpeedBase - currentBase;
            if (std::abs(delta) > 0.1f) {
                owner->ModActorValue(RE::ActorValue::kSpeedMult, delta);
                spdlog::info("LifeAgain: {} hizi sifirlandi: {:.1f} -> {:.1f} (delta: {:.1f})",
                    actor->GetDisplayFullName(), currentBase, inf->savedSpeedBase, delta);
            }
        }

        // Yaralı animasyon durumunu sıfırla - bu olmadan NPC topallmaya devam eder
        actor->NotifyAnimationGraph("IdleWoundedStop");
        spdlog::info("LifeAgain: Injury debuffs removed from {} (savedSpeedBase restore)", actor->GetDisplayFullName());
    }

    // Avoid Actor::GetGoldAmount() for custom followers. That helper filters every
    // inventory entry by dereferencing its form; some modded followers expose a
    // stale entry that can crash there. Count Septims directly by pointer instead.
    inline int GetSafeActorGoldCount(RE::Actor* actor)
    {
        if (!actor) return 0;
        auto* gold = static_cast<RE::TESBoundObject*>(RE::TESForm::LookupByID(0x0000000F));
        auto* base = actor->GetActorBase();
        if (!gold || !base) return 0;

        int count = base->CountObjectsInContainer(gold);
        if (auto* changes = actor->GetInventoryChanges(true)) {
            count += changes->GetItemCount(gold);
        }
        return std::max(0, count);
    }

    inline void ApplyInjuryDebuffs(RE::Actor* actor, InjuryLevel oldLevel, InjuryLevel newLevel, InjuredFollower* inf = nullptr) {
        if (!actor) return;
        // Önce eskileri geri al (eğer varsa)
        if (oldLevel != InjuryLevel::None) {
            RemoveInjuryDebuffs(actor, oldLevel);
        }
        
        if (newLevel != InjuryLevel::None) {
            auto owner = actor->AsActorValueOwner();
            if (owner) {
                // Yaralanmadan onceki baz hizi kaydet (iyilesince tam geri yuklenir)
                if (inf && oldLevel == InjuryLevel::None) {
                    inf->savedSpeedBase = owner->GetBaseActorValue(RE::ActorValue::kSpeedMult);
                    spdlog::info("LifeAgain: {} savedSpeedBase kaydedildi: {:.1f}", actor->GetDisplayFullName(), inf->savedSpeedBase);
                }

                float damageMod = (newLevel == InjuryLevel::Heavy) ? -0.25f : -0.10f;
                // Heavy followers retain a slow walking pace; OAR replaces their
                // run/sprint clips with matching wounded walk clips.
                float speedMod  = (newLevel == InjuryLevel::Heavy) ? -45.0f : -10.0f;
                float regenMod  = (newLevel == InjuryLevel::Heavy) ? -25.0f : -15.0f;
                float armorMod  = (newLevel == InjuryLevel::Heavy) ? -60.0f : -30.0f;

                owner->ModActorValue(RE::ActorValue::kAttackDamageMult, damageMod);
                owner->ModActorValue(RE::ActorValue::kSpeedMult, speedMod);
                owner->ModActorValue(RE::ActorValue::kHealRateMult, regenMod);
                owner->ModActorValue(RE::ActorValue::kMagickaRateMult, regenMod);
                owner->ModActorValue(RE::ActorValue::kStaminaRateMult, regenMod);
                owner->ModActorValue(RE::ActorValue::kDamageResist, armorMod);
            }
            spdlog::info("LifeAgain: {} is now injured (Level: {}). SpeedMod & DamageMod applied.", actor->GetDisplayFullName(), (int)newLevel);
            
            // Inleme sesi caldir ve animasyon yolla
            auto* base = actor->GetActorBase();
            bool isFemale = base && (base->GetSex() == 1);
            if (isFemale) {
                RE::PlaySound("NPCHumanFemaleGroan");
            } else {
                RE::PlaySound("NPCHumanMaleGroan");
            }
            actor->NotifyAnimationGraph("IdleWounded");

            // Bildirimler
            std::string msg;
            if (newLevel == InjuryLevel::Light) {
                msg = fmt::format("{} is lightly injured, you should heal them. ({} hafif yaralandi, tedavi ettirsen iyi olur.)", actor->GetDisplayFullName(), actor->GetDisplayFullName());
            } else {
                msg = fmt::format("{} is heavily injured, heal them immediately! ({} agir yaralandi, acilen tedavi ettir!)", actor->GetDisplayFullName(), actor->GetDisplayFullName());
            }

            if (g_Settings.ShowInjuryNotification) {
                RE::DebugNotification(msg.c_str());
            }
            if (g_Settings.ShowInjuryMessageBox) {
                RE::DebugMessageBox(msg.c_str());
            }
        }
    }


    // Save loading relies on the injury snapshot to restore scars/OAR state; no bandage gear is used.
    inline void RefreshInjuryBridgeOnLoad() {
        PublishFollowerInjurySnapshot();
    }

    inline void ProcessNaturalLightWoundHealing(float a_eventGameDay = -1.0f)
    {
        constexpr float recoveryDaysPerLightWound = 3.0f;
        auto* calendar = RE::Calendar::GetSingleton();
        if (!calendar) return;
        const float currentGameDay = a_eventGameDay >= 0.0f ? a_eventGameDay : calendar->GetDaysPassed();
        auto trace = spdlog::get("LifeAgainTreatment");
        auto& tracker = FollowerTracker::GetSingleton();

        for (auto record : tracker.GetInjuredFollowers()) {
            const auto lightWounds = std::accumulate(record.injuryCounts.begin(), record.injuryCounts.begin() + 4, 0u);
            if (record.recoveryLastGameDay < 0.0f || currentGameDay < record.recoveryLastGameDay) {
                record.recoveryLastGameDay = currentGameDay;
                record.recoveryProgressDays = 0.0f;
                tracker.UpdateInjured(record.formID, [record](InjuredFollower& current) {
                    current.recoveryLastGameDay = record.recoveryLastGameDay;
                    current.recoveryProgressDays = record.recoveryProgressDays;
                });
                continue;
            }

            const float elapsed = currentGameDay - record.recoveryLastGameDay;
            record.recoveryLastGameDay = currentGameDay;
            if (lightWounds == 0) {
                record.recoveryProgressDays = 0.0f;
                tracker.UpdateInjured(record.formID, [record](InjuredFollower& current) {
                    current.recoveryLastGameDay = record.recoveryLastGameDay;
                    current.recoveryProgressDays = record.recoveryProgressDays;
                });
                continue;
            }

            record.recoveryProgressDays += elapsed;
            bool healedAny = false;
            static std::mt19937 rng{ std::random_device{}() };
            while (record.recoveryProgressDays >= recoveryDaysPerLightWound) {
                const auto remainingLight = std::accumulate(record.injuryCounts.begin(), record.injuryCounts.begin() + 4, 0u);
                if (remainingLight == 0) {
                    record.recoveryProgressDays = 0.0f;
                    break;
                }
                std::uniform_int_distribution<std::uint32_t> woundRoll(0, remainingLight - 1);
                auto roll = woundRoll(rng);
                std::size_t type = 0;
                for (; type < 4; ++type) {
                    if (roll < record.injuryCounts[type]) break;
                    roll -= record.injuryCounts[type];
                }
                if (type >= 4 || record.injuryCounts[type] == 0) break;
                --record.injuryCounts[type];
                record.recoveryProgressDays -= recoveryDaysPerLightWound;
                healedAny = true;
                if (trace) trace->info("Natural recovery healed one light wound: follower={:08X}, type={}, gameDay={:.2f}.",
                    record.formID, kInjuryTypeNames[type], currentGameDay);
            }

            const auto oldLevel = record.injuryLevel;
            record.injuryLevel = GetInjuryLevel(record.injuryCounts);
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(record.formID);
            if (healedAny && record.injuryLevel == InjuryLevel::None) {
                auto previous = tracker.GetInjured(record.formID);
                if (actor && previous) RemoveInjuryDebuffs(actor, &*previous);
                tracker.RemoveInjured(record.formID);
                if (actor) RE::DebugNotification(fmt::format("{} has recovered from all injuries. ({} tamamen iyilesti.)", record.name, record.name).c_str());
            } else {
                if (actor && oldLevel != record.injuryLevel) {
                    ApplyInjuryDebuffs(actor, oldLevel, record.injuryLevel);
                }
                if (healedAny) {
                    tracker.AddOrUpdateInjured(record);
                } else {
                    tracker.UpdateInjured(record.formID, [record](InjuredFollower& current) {
                        current.recoveryLastGameDay = record.recoveryLastGameDay;
                        current.recoveryProgressDays = record.recoveryProgressDays;
                    });
                }
            }
        }
    }

    // Oyun içi zamanı string'e çevirir
    inline std::string GetCurrentGameTimeString() {
        auto* calendar = RE::Calendar::GetSingleton();
        if (!calendar) return "Unknown";
        int h  = static_cast<int>(calendar->GetHour());
        int d  = static_cast<int>(calendar->GetDay());
        int m  = static_cast<int>(calendar->GetMonth());
        int y  = static_cast<int>(calendar->GetYear());
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:00", d, m, y, h);
        return buf;
    }

    // Aktörün hangi yoldas tipine girdiğini bul
    inline FollowerKind DetectKind(RE::Actor* actor) {
        if (!actor) return FollowerKind::Other;
        auto* race = actor->GetRace();
        if (!race) return FollowerKind::Human;

        std::string raceEdid = race->GetFormEditorID() ? race->GetFormEditorID() : "";

        if (raceEdid.find("Horse") != std::string::npos ||
            raceEdid.find("horse") != std::string::npos)
            return FollowerKind::Horse;
        if (raceEdid.find("Dog") != std::string::npos ||
            raceEdid.find("dog") != std::string::npos ||
            raceEdid.find("Wolf") != std::string::npos)
            return FollowerKind::Dog;
        if (raceEdid.find("Cat") != std::string::npos ||
            raceEdid.find("cat") != std::string::npos ||
            raceEdid.find("Khajiit") != std::string::npos ||
            raceEdid.find("khajiit") != std::string::npos)
            return FollowerKind::Cat;

        return FollowerKind::Human;
    }

    // Aktörün şu an aktif olarak oyuncuyu takip edip etmediğini kontrol et
    inline bool IsFollower(RE::Actor* actor) {
        if (!actor || actor->IsPlayerRef()) return false;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;

        // Muhafizlar, cagrili yaratiklar veya gecici komuta edilenler kesinlikle takipci degildir
        if (actor->IsGuard() || actor->IsSummoned() || actor->IsCommandedActor()) {
            return false;
        }

        // Oyuncuya dusmansa asla yoldas degildir
        if (actor->IsHostileToActor(player)) return false;

        // Mesafe / hucre kontrolü: Takipçi oyuncuyla ayni hucrede ve yakin mesafede olmali
        if (actor->GetParentCell() != player->GetParentCell()) {
            return false;
        }
        // 8000 birim (~130 metre) uzerindeki aktörler aktif takipçi sayilmaz
        if (actor->GetPosition().GetDistance(player->GetPosition()) > 8000.0f) {
            return false;
        }

        // 1) En kesin tespit: IsPlayerTeammate() kontrolü
        // (Vanilla yoldaslar, NFF, AFT, EFF, Inigo, Lucien aktif takipteyken kesinlikle true olur)
        if (actor->IsPlayerTeammate()) {
            return true;
        }

        // 2) Vanilla CurrentFollowerFaction (0x0005C84D) kontrolü
        // Sadece bu faction'a gercekten dahilse ve rutbesi >= 0 ise
        auto* followerFaction = static_cast<RE::TESFaction*>(RE::TESForm::LookupByID(0x5C84D));
        if (followerFaction && actor->IsInFaction(followerFaction)) {
            if (actor->GetFactionRank(followerFaction, false) >= 0) {
                return true;
            }
        }

        // 3) Kopek ve evcil hayvan takipcileri (CurrentDogFaction: 0x000DAB74)
        auto* dogFaction = static_cast<RE::TESFaction*>(RE::TESForm::LookupByID(0xDAB74));
        if (dogFaction && actor->IsInFaction(dogFaction)) {
            if (actor->GetFactionRank(dogFaction, false) >= 0) {
                return true;
            }
        }

        // 4) Parali Asker Takipcileri (CurrentHirelingFaction: 0x000918E2)
        auto* hirelingFaction = static_cast<RE::TESFaction*>(RE::TESForm::LookupByID(0x918E2));
        if (hirelingFaction && actor->IsInFaction(hirelingFaction)) {
            if (actor->GetFactionRank(hirelingFaction, false) >= 0) {
                return true;
            }
        }

        return false;
    }

    // Injuries are designed for humanoid party followers. Animal companions
    // can still be recognized by the follower/death system, but never receive
    // Life Again wounds or enter the injury test lab.
    inline bool IsInjuryEligibleFollower(RE::Actor* actor) {
        if (!actor || actor->IsPlayerRef() || actor->IsDead() || actor->IsGuard() ||
            actor->IsSummoned() || actor->IsCommandedActor() || !actor->IsPlayerTeammate()) return false;
        const auto* race = actor->GetRace();
        if (!race || !race->data.flags.any(RE::RACE_DATA::Flag::kPlayable)) return false;
        const char* raceEditorID = race->GetFormEditorID();
        if (raceEditorID) {
            const std::string_view raceName{ raceEditorID };
            if (raceName.find("Khajiit") != std::string_view::npos ||
                raceName.find("khajiit") != std::string_view::npos) return false;
        }
        return true;
    }

    // ---- Aktörün Rahip olup olmadığını kontrol et ----
    // Skyrim.esm PriestFaction: 0x00013350
    // Yedek: NPC adında "Priest", "Preist", "Priestess" geçiyorsa da kabul et
    inline bool IsPriest(RE::Actor* actor) {
        if (!actor) return false;

        // 1) PriestFaction kontrolü (0x00013350 - Skyrim.esm)
        auto* priestFaction = static_cast<RE::TESFaction*>(RE::TESForm::LookupByID(0x13350));
        if (priestFaction && actor->IsInFaction(priestFaction)) {
            spdlog::debug("LifeAgain: {} PriestFaction uyesi", actor->GetDisplayFullName());
            return true;
        }

        // 2) WIKill_03 rahibi (vanilla): WIKill03PriestFaction 0x17CA6
        auto* tempPriestFaction = static_cast<RE::TESFaction*>(RE::TESForm::LookupByID(0x17CA6));
        if (tempPriestFaction && actor->IsInFaction(tempPriestFaction)) {
            spdlog::debug("LifeAgain: {} TemplePriestFaction uyesi", actor->GetDisplayFullName());
            return true;
        }

        // 3) JobPriestFaction kontrolü (0x0005159A) -> Danica vb. için
        auto* jobPriestFaction = static_cast<RE::TESFaction*>(RE::TESForm::LookupByID(0x5159A));
        if (jobPriestFaction && actor->IsInFaction(jobPriestFaction)) {
            spdlog::debug("LifeAgain: {} JobPriestFaction uyesi", actor->GetDisplayFullName());
            return true;
        }

        // 4) NPC adında "Priest" / "Priestess" / "Preist" geçiyor mu?
        std::string name = actor->GetDisplayFullName();
        if (!name.empty()) {
            // Küçük harfe çevir
            std::string lower = name;
            for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lower.find("priest") != std::string::npos ||
                lower.find("rahip") != std::string::npos ||
                lower.find("rahibe") != std::string::npos ||
                lower.find("cleric") != std::string::npos ||
                lower.find("arkay") != std::string::npos ||
                lower.find("vigilant") != std::string::npos ||
                lower.find("stendarr") != std::string::npos ||
                lower.find("talos") != std::string::npos ||
                lower.find("dibella") != std::string::npos ||
                lower.find("kynareth") != std::string::npos ||
                lower.find("zenithar") != std::string::npos ||
                lower.find("akatosh") != std::string::npos ||
                lower.find("julianos") != std::string::npos ||
                lower.find("monk") != std::string::npos ||
                lower.find("kesis") != std::string::npos ||
                lower.find("keşiş") != std::string::npos ||
                lower.find("andurs") != std::string::npos ||
                lower.find("runil") != std::string::npos ||
                lower.find("danica") != std::string::npos ||
                lower.find("maramal") != std::string::npos ||
                lower.find("erandur") != std::string::npos) {
                spdlog::debug("LifeAgain: {} ismiyle rahip tespit edildi", name);
                return true;
            }
        }

        return false;
    }

    // ---- Aktörün tapınak lokasyonunda olup olmadığını kontrol et ----
    // LocTypeTemple keyword: 0x0001B41D (Skyrim.esm)
    inline bool IsInTemple(RE::Actor* actor) {
        if (!actor) return false;
        auto* location = actor->GetCurrentLocation();
        if (!location) return false;

        // BGSLocation, BGSKeywordForm'dan türüyor → HasKeywordID kullanabiliriz
        if (location->HasKeywordID(0x1B41D)) {
            spdlog::debug("LifeAgain: Tapınak lokasyonu dogrulandi: {}", 
                location->GetFullName() ? location->GetFullName() : "Unknown");
            return true;
        }

        // Yedek: Tapınak adında "Temple", "Shrine", "Tapınak" geçiyor mu?
        const char* locName = location->GetFullName();
        if (locName) {
            std::string lower = locName;
            for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lower.find("temple") != std::string::npos ||
                lower.find("shrine") != std::string::npos ||
                lower.find("hall of the dead") != std::string::npos ||
                lower.find("oluler salonu") != std::string::npos ||
                lower.find("ölüler salonu") != std::string::npos ||
                lower.find("arkay") != std::string::npos ||
                lower.find("tapinak") != std::string::npos) {
                spdlog::debug("LifeAgain: Tapınak ismiyle tespit edildi: {}", locName);
                return true;
            }
        }

        return false;
    }

    // ---- Ölüm Event Dinleyicisi ----
    class DeathEventSink : public RE::BSTEventSink<RE::TESDeathEvent> {
    public:
        static DeathEventSink* GetSingleton() {
            static DeathEventSink instance;
            return &instance;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESDeathEvent*     event,
            RE::BSTEventSource<RE::TESDeathEvent>* /*source*/) override
        {
            if (!event || !event->actorDying) return RE::BSEventNotifyControl::kContinue;

            auto* actor = event->actorDying->As<RE::Actor>();
            if (!actor) return RE::BSEventNotifyControl::kContinue;
            if (!IsFollower(actor)) return RE::BSEventNotifyControl::kContinue;

            DeadFollower df;
            df.formID    = actor->GetFormID();
            df.name      = actor->GetDisplayFullName();
            df.level     = actor->GetLevel();
            df.kind      = DetectKind(actor);
            df.deathDate = GetCurrentGameTimeString();

            // Zaten listede varsa tekrar işleme alma
            auto deadList = FollowerTracker::GetSingleton().GetDeadFollowers();
            for (const auto& d : deadList) {
                if (d.formID == df.formID) return RE::BSEventNotifyControl::kContinue;
            }

            // Eğer yaralı listesindeyse temizle (artık öldü, dashboard'da yaralı gözükmemeli)
            // AV/bandaj çağrıları event sink'te değil task interface'te yapılır (crash güvenliği)
            auto* tracker = &FollowerTracker::GetSingleton();
            auto injuredOpt = tracker->GetInjured(df.formID);
            InjuryLevel dyingLevel = injuredOpt ? injuredOpt->injuryLevel : InjuryLevel::None;

            spdlog::info("LifeAgain: Follower died -> {} (Level {}) at {}", df.name, df.level, df.deathDate);
            // Mesaj, df move edilmeden ÖNCE oluşturulmalı (move sonrası df.name tanımsız)
            std::string msg = "Your follower " + df.name + " has died! (Yoldasiniz " + df.name + " oldu!)";
            const RE::FormID deadFormID = df.formID;

            FollowerTracker::GetSingleton().AddDead(std::move(df));

            auto deadHandle = actor->GetHandle();
            if (auto* taskInterface = SKSE::GetTaskInterface()) {
                taskInterface->AddTask([deadHandle, dyingLevel, deadFormID]() {
                    auto actorPtr = deadHandle.get();
                    if (actorPtr && dyingLevel != InjuryLevel::None) {
                        auto* a = actorPtr.get();
                        if (a) {
                            RemoveInjuryDebuffs(a, dyingLevel);
                        }
                    }
                    FollowerTracker::GetSingleton().RemoveInjured(deadFormID);
                });
            } else {
                tracker->RemoveInjured(deadFormID);
            }
            if (g_Settings.ShowDeathNotification) {
                RE::DebugNotification(msg.c_str());
            }
            if (g_Settings.ShowDeathMessageBox) {
                RE::DebugMessageBox(msg.c_str());
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    // ---- Aktivasyon Event Dinleyicisi (Rahip tespiti) ----
    class ActivateEventSink : public RE::BSTEventSink<RE::TESActivateEvent> {
    public:
        static ActivateEventSink* GetSingleton() {
            static ActivateEventSink instance;
            return &instance;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESActivateEvent*     event,
            RE::BSTEventSource<RE::TESActivateEvent>* /*source*/) override
        {
            if (!event) return RE::BSEventNotifyControl::kContinue;

            // actionRef = kim aktive etti?
            // objectActivated = ne aktive edildi?
            auto* activator = event->actionRef ? event->actionRef->As<RE::Actor>() : nullptr;
            auto* target    = event->objectActivated ? event->objectActivated->As<RE::Actor>() : nullptr;

            // Sadece oyuncu bir NPC'yi aktive ettiyse işlem yap
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!activator || activator != player) return RE::BSEventNotifyControl::kContinue;
            if (!target) return RE::BSEventNotifyControl::kContinue;

            bool priest  = IsPriest(target);

            if (priest) {
                g_PriestAccess = true;
                g_PriestName   = target->GetDisplayFullName();
                spdlog::info("LifeAgain: Rahip erişimi verildi -> {}", g_PriestName);
                // Priest access only unlocks the player's manual healing/revival menus.
            } else {
                // Rahip değilse erişimi sıfırla
                // (Menü ancak tekrar rahiple konuşunca açılabilir)
                if (g_PriestAccess.load()) {
                    g_PriestAccess = false;
                    spdlog::info("LifeAgain: Rahip erişimi sıfırlandı (rahip/tapınak dışı aktivasyon)");
                }
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    inline void RecordBattleWound(RE::Actor* a_actor, InjuryType a_type)
    {
        auto trace = spdlog::get("LifeAgainTreatment");
        if (!IsInjuryEligibleFollower(a_actor) || a_actor->IsDead() ||
            static_cast<std::size_t>(a_type) >= kInjuryTypeCount) return;
        const auto formID = a_actor->GetFormID();
        if (trace) trace->info("Wound application requested: actor={:08X} ({}), type={}",
            formID, a_actor->GetDisplayFullName(), kInjuryTypeNames[static_cast<std::size_t>(a_type)]);
        auto& tracker = FollowerTracker::GetSingleton();
        auto current = tracker.GetInjured(formID);
        InjuredFollower updated;
        const InjuryLevel oldLevel = current ? current->injuryLevel : InjuryLevel::None;
        if (current) {
            updated = *current;
        } else {
            updated.formID = formID;
            updated.name = a_actor->GetDisplayFullName();
            updated.level = a_actor->GetLevel();
            updated.kind = DetectKind(a_actor);
            if (auto* calendar = RE::Calendar::GetSingleton()) {
                updated.recoveryLastGameDay = calendar->GetDaysPassed();
            }
        }

        const auto index = static_cast<std::size_t>(a_type);
        if (index >= updated.injuryCounts.size()) return;
        if (index < 4 && std::accumulate(updated.injuryCounts.begin(), updated.injuryCounts.begin() + 4, 0u) == 0) {
            if (auto* calendar = RE::Calendar::GetSingleton()) updated.recoveryLastGameDay = calendar->GetDaysPassed();
            updated.recoveryProgressDays = 0.0f;
        }
        const auto total = std::accumulate(updated.injuryCounts.begin(), updated.injuryCounts.end(), 0u);
        if (total >= 12 || updated.injuryCounts[index] >= 10) return;
        ++updated.injuryCounts[index];
        updated.injuryLevel = GetInjuryLevel(updated.injuryCounts);
        const InjuryLevel newLevel = updated.injuryLevel;
        if (trace) trace->info("Wound record prepared: actor={:08X}, previousLevel={}, newLevel={}, totalWounds={}",
            formID, static_cast<int>(oldLevel), static_cast<int>(newLevel), total + 1);
        if (trace) trace->info("Updating tracker and publishing injury visuals: actor={:08X}.", formID);
        tracker.AddOrUpdateInjured(updated);
        if (trace) trace->info("Tracker update and visual publication returned: actor={:08X}.", formID);

        auto* tasks = SKSE::GetTaskInterface();
        const auto handle = a_actor->GetHandle();
        const auto capturedFormID = formID;
        const auto name = updated.name;
        if (tasks) {
            if (trace) trace->info("Scheduling wound gameplay effects: actor={:08X}.", formID);
            tasks->AddTask([handle, capturedFormID, oldLevel, newLevel, name, a_type]() {
                auto taskTrace = spdlog::get("LifeAgainTreatment");
                if (taskTrace) taskTrace->info("Wound gameplay task started: actor={:08X}.", capturedFormID);
                auto actorPtr = handle.get();
                auto* actor = actorPtr ? actorPtr.get() : nullptr;
                if (!actor || actor->IsDead()) {
                    if (taskTrace) taskTrace->error("Wound gameplay task aborted: actor handle invalid or actor dead; actor={:08X}.", capturedFormID);
                    return;
                }

                if (oldLevel != newLevel) {
                    auto currentRecord = FollowerTracker::GetSingleton().GetInjured(capturedFormID);
                    if (currentRecord) {
                        auto copy = *currentRecord;
                        if (taskTrace) taskTrace->info("Applying injury debuffs: actor={:08X}.", capturedFormID);
                        ApplyInjuryDebuffs(actor, oldLevel, newLevel, &copy);
                        if (taskTrace) taskTrace->info("Injury debuffs applied: actor={:08X}.", capturedFormID);
                        const float savedSpeed = copy.savedSpeedBase;
                        FollowerTracker::GetSingleton().UpdateInjured(capturedFormID, [savedSpeed](InjuredFollower& record) {
                            record.savedSpeedBase = savedSpeed;
                        });
                    }
                }

                if (auto* base = actor->GetActorBase(); base && base->GetSex() == 1) RE::PlaySound("NPCHumanFemaleGroan");
                else RE::PlaySound("NPCHumanMaleGroan");
                if (taskTrace) taskTrace->info("Playing wound reaction and requesting IdleWounded: actor={:08X}.", capturedFormID);
                actor->NotifyAnimationGraph("IdleWounded");
                const auto msg = fmt::format("{} suffered {}.", name, kInjuryTypeNames[static_cast<std::size_t>(a_type)]);
                if (g_Settings.ShowInjuryNotification) RE::DebugNotification(msg.c_str());
                if (g_Settings.ShowInjuryMessageBox) RE::DebugMessageBox(msg.c_str());
                // Life Again never purchases medicine. If the follower already has
                // enough potions, use those; otherwise wait for inventory changes
                // (for example, after Follower Unbound supplies the potions).
                auto injury = FollowerTracker::GetSingleton().GetInjured(capturedFormID);
                if (injury) CheckPotionHealing(actor, &*injury);
                if (taskTrace) taskTrace->info("Wound gameplay task completed: actor={:08X}.", capturedFormID);
            });
        } else if (trace) {
            trace->error("Could not schedule wound gameplay task: SKSE task interface unavailable; actor={:08X}.", formID);
        }
    }

    class HitEventSink : public RE::BSTEventSink<RE::TESHitEvent> {
    public:
        static HitEventSink* GetSingleton() { static HitEventSink instance; return &instance; }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* event, RE::BSTEventSource<RE::TESHitEvent>*) override
        {
            if (!event || !event->target || !event->cause) return RE::BSEventNotifyControl::kContinue;
            auto* actor = event->target->As<RE::Actor>();
            auto* aggressor = event->cause->As<RE::Actor>();
            if (!actor || !aggressor || aggressor == actor || !IsInjuryEligibleFollower(actor) || actor->IsDead()) return RE::BSEventNotifyControl::kContinue;

            const auto now = std::chrono::steady_clock::now();
            {
                std::scoped_lock lock(g_hitMutex);
                const auto it = g_lastHitTime.find(actor->GetFormID());
                if (it != g_lastHitTime.end() && now - it->second < std::chrono::seconds(30)) return RE::BSEventNotifyControl::kContinue;
                g_lastHitTime[actor->GetFormID()] = now;
            }

            static std::mt19937 rng{ std::random_device{}() };
            std::scoped_lock rngLock(g_rngMutex);
            std::uniform_int_distribution<int> roll(1, 100);
            InjuryLevel severity = InjuryLevel::None;
            if (roll(rng) <= std::clamp(g_Settings.HeavyInjuryChance, 0, 100)) {
                severity = InjuryLevel::Heavy;
            } else if (roll(rng) <= std::clamp(g_Settings.LightInjuryChance, 0, 100)) {
                severity = InjuryLevel::Light;
            }
            if (severity == InjuryLevel::None) return RE::BSEventNotifyControl::kContinue;

            const int firstType = severity == InjuryLevel::Light ? 0 : 4;
            std::uniform_int_distribution<int> typeRoll(firstType, firstType + 3);
            RecordBattleWound(actor, static_cast<InjuryType>(typeRoll(rng)));
            return RE::BSEventNotifyControl::kContinue;
        }

    private:
        std::mutex g_hitMutex;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> g_lastHitTime;
        std::mutex g_rngMutex;
    };

    // ---- Bleedout (Yere Düşme) Ölüm Şansı Dinleyicisi ----
    class BleedoutEventSink : public RE::BSTEventSink<RE::TESEnterBleedoutEvent> {
    public:
        static BleedoutEventSink* GetSingleton() {
            static BleedoutEventSink instance;
            return &instance;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESEnterBleedoutEvent* event,
            RE::BSTEventSource<RE::TESEnterBleedoutEvent>* /*source*/) override
        {
            if (!event || !event->actor) return RE::BSEventNotifyControl::kContinue;

            auto* actor = event->actor->As<RE::Actor>();
            if (!actor || !IsFollower(actor)) return RE::BSEventNotifyControl::kContinue;

            RE::FormID formID = actor->GetFormID();
            auto* tracker = &FollowerTracker::GetSingleton();
            auto injuredOpt = tracker->GetInjured(formID);
            InjuredFollower* inf = injuredOpt ? &*injuredOpt : nullptr;

            int chance = g_Settings.BleedoutDeathChance_Healthy;
            if (inf) {
                if (inf->injuryLevel == InjuryLevel::Light) chance = g_Settings.BleedoutDeathChance_Light;
                else if (inf->injuryLevel == InjuryLevel::Heavy) chance = g_Settings.BleedoutDeathChance_Heavy;
            }

            static std::mt19937 rng{ std::random_device{}() };
            std::uniform_int_distribution<int> dist(1, 100);

            // Injury chance is handled on hit events; bleedout only governs death risk.
            bool died = chance > 0 && dist(rng) <= chance;
            if (died) {
                spdlog::info("LifeAgain: Bleedout olum sansi tetiklendi ({}/100) -> {}", chance, actor->GetDisplayFullName());

                auto actorHandle = actor->GetHandle();
                auto injuryLevel = inf ? inf->injuryLevel : InjuryLevel::None;

                if (auto* taskInterface = SKSE::GetTaskInterface()) {
                    taskInterface->AddTask([actorHandle, injuryLevel]() {
                        auto actorPtr = actorHandle.get();
                        if (!actorPtr) return;
                        auto* a = actorPtr.get();
                        if (!a) return;

                        // NPC essential veya protected ise bunlari temizle
                        auto* base = a->GetActorBase();
                        if (base) {
                            base->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kEssential);
                            base->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kProtected);
                        }
                        auto& runtimeData = a->GetActorRuntimeData();
                        runtimeData.boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
                        runtimeData.boolFlags.reset(RE::Actor::BOOL_FLAGS::kProtected);

                        // Debuff'i sil
                        if (injuryLevel != InjuryLevel::None) {
                            RemoveInjuryDebuffs(a, injuryLevel);
                            FollowerTracker::GetSingleton().RemoveInjured(a->GetFormID());
                        }

                        // Öldür
                        a->KillImmediate();
                        a->NotifyAnimationGraph("Ragdoll");
                    });
                }
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    inline void CheckPotionHealing(RE::Actor* actor, InjuredFollower* inf) {
        if (!actor || actor->IsDead() || !inf) return;
        auto trace = spdlog::get("LifeAgainTreatment");
        if (trace) trace->info("Potion treatment check: actor={:08X} ({})", actor->GetFormID(), actor->GetDisplayFullName());

        static const RE::FormID cureDiseaseFormID = 0x000AE723;
        // LookupByID<T> -> As<T>() lib'de bulunmuyor; raw lookup + formType kontrolü kullan
        auto* potionFormBase = RE::TESForm::LookupByID(cureDiseaseFormID);
        if (!potionFormBase || potionFormBase->GetFormType() != RE::FormType::AlchemyItem) return;
        auto* potionForm = static_cast<RE::AlchemyItem*>(potionFormBase);

        // Actor envanterindeki iksir sayısını al
        int potionCount = 0;
        {
            auto inv = actor->GetInventory([cureFID = cureDiseaseFormID](RE::TESBoundObject& obj) {
                return obj.GetFormID() == cureFID;
            });
            for (auto& [form, data] : inv) {
                if (form && form->GetFormID() == cureDiseaseFormID) {
                    potionCount = data.first;
                    break;
                }
            }
        }
        const auto woundTotal = std::accumulate(inf->injuryCounts.begin(), inf->injuryCounts.end(), 0u);
        if (woundTotal == 0) return;
        if (trace) trace->info("Potion treatment inventory: actor={:08X} carried={}, wounds={}", actor->GetFormID(), potionCount, woundTotal);
        if (potionCount < 1) {
            if (trace) trace->info("Potion treatment deferred: actor={:08X} has no Cure Disease Potions.", actor->GetFormID());
            return;
        }

        if (trace) trace->info("Calling Actor::DrinkPotion for one-dose partial treatment of actor={:08X}.", actor->GetFormID());
        if (!actor->DrinkPotion(potionForm, nullptr)) {
            if (trace) trace->error("Actor::DrinkPotion failed for actor={:08X}; wounds retained.", actor->GetFormID());
            return;
        }

        // Pick one wound instance uniformly, so types with multiple wounds have
        // proportionally higher odds while every individual wound is equally likely.
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<std::uint32_t> woundRoll(0, woundTotal - 1);
        auto roll = woundRoll(rng);
        std::size_t healedType = 0;
        for (; healedType < inf->injuryCounts.size(); ++healedType) {
            const auto count = inf->injuryCounts[healedType];
            if (roll < count) break;
            roll -= count;
        }
        if (healedType >= inf->injuryCounts.size() || inf->injuryCounts[healedType] == 0) {
            if (trace) trace->error("Random wound selection failed for actor={:08X}; potion was consumed, injury record retained.", actor->GetFormID());
            return;
        }

        const auto oldLevel = inf->injuryLevel;
        auto updated = *inf;
        --updated.injuryCounts[healedType];
        updated.injuryLevel = GetInjuryLevel(updated.injuryCounts);
        if (trace) trace->info("Potion healed one random wound: actor={:08X}, type={}, remainingWounds={}, level={}->{}.",
            actor->GetFormID(), kInjuryTypeNames[healedType], woundTotal - 1,
            static_cast<int>(oldLevel), static_cast<int>(updated.injuryLevel));

        auto& tracker = FollowerTracker::GetSingleton();
        if (updated.injuryLevel == InjuryLevel::None) {
            RemoveInjuryDebuffs(actor, inf);
            tracker.RemoveInjured(inf->formID);
        } else {
            if (updated.injuryLevel != oldLevel) {
                ApplyInjuryDebuffs(actor, oldLevel, updated.injuryLevel);
            }
            tracker.AddOrUpdateInjured(updated);
        }

        const auto message = fmt::format("{} used a Cure Disease Potion to heal one wound: {}. ({} bir yarasını iyileştirdi.)",
            inf->name, kInjuryTypeNames[healedType], inf->name);
        RE::DebugNotification(message.c_str());
        if (trace) trace->info("Potion partial treatment completed for actor={:08X}; remainingWounds={}.",
            inf->formID, woundTotal - 1);
    }

    // ---- Container Changed Event (Iksir Kontrolu) ----
    class ContainerChangedEventSink : public RE::BSTEventSink<RE::TESContainerChangedEvent> {
    public:
        static ContainerChangedEventSink* GetSingleton() {
            static ContainerChangedEventSink instance;
            return &instance;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
            if (!event || event->newContainer == 0) return RE::BSEventNotifyControl::kContinue;

            auto* tracker = &FollowerTracker::GetSingleton();
            auto injuredOpt = tracker->GetInjured(event->newContainer);
            if (injuredOpt) {
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(event->newContainer);
                if (actor) {
                    // Cagiriyi asenkron yapalim ki envanter tam guncellenmis olsun
                    RE::FormID formID = event->newContainer;
                    if (auto* task = SKSE::GetTaskInterface()) {
                        task->AddTask([formID]() {
                            auto* a = RE::TESForm::LookupByID<RE::Actor>(formID);
                            auto* tr = &FollowerTracker::GetSingleton();
                            auto i = tr->GetInjured(formID);
                            if (a && i) CheckPotionHealing(a, &*i);
                        });
                    }
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    inline void RegisterDeathEventSink() {
        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (holder) {
            holder->AddEventSink(DeathEventSink::GetSingleton());
            holder->AddEventSink(ActivateEventSink::GetSingleton());
            holder->AddEventSink(BleedoutEventSink::GetSingleton());
            holder->AddEventSink(HitEventSink::GetSingleton());
            holder->AddEventSink(ContainerChangedEventSink::GetSingleton());
            spdlog::info("LifeAgain: Event Sinks registered successfully.");
        }
    }
}

