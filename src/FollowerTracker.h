#pragma once
#include <SKSE/SKSE.h>
#undef PlaySound
#include <random>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <atomic>
#include <spdlog/spdlog.h>
#include <RE/S/ScriptEventSourceHolder.h>
#include <RE/C/Calendar.h>
#include <RE/T/TESRace.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESDeathEvent.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESActivateEvent.h>
#include <RE/T/TESEnterBleedoutEvent.h>
#include <RE/T/TESContainerChangedEvent.h>
#include <RE/T/TESWaitStopEvent.h>
#include <RE/T/TESSleepStopEvent.h>
#include <RE/T/TESFastTravelEndEvent.h>
#include <RE/M/Misc.h>
#include <RE/B/BGSLocation.h>
#include <RE/B/BGSKeyword.h>
#include <RE/I/InventoryChanges.h>
#include <RE/A/AlchemyItem.h>
#include <RE/T/TESDataHandler.h>
#include <RE/A/ActorEquipManager.h>
#include <RE/T/TESObjectARMO.h>

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

    struct InjuredFollower {
        RE::FormID formID = 0;
        std::string name;
        int level = 1;
        FollowerKind kind = FollowerKind::Human;
        InjuryLevel injuryLevel = InjuryLevel::Light;
        int bleedoutCount = 0;
        float injuryStartDay = 0.0f;  // Yaralanma anındaki oyun günü (GetDaysPassed)
        float lastBleedoutDay = 0.0f; // En son bleedout günü (3 gün sayacı sıfırlama)
    };

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
            std::lock_guard lock(m_mutex);
            for (auto& i : m_injured) {
                if (i.formID == inv.formID) {
                    i.injuryLevel = inv.injuryLevel;
                    i.bleedoutCount = inv.bleedoutCount;
                    i.lastBleedoutDay = inv.lastBleedoutDay;
                    if (i.injuryStartDay == 0.0f) i.injuryStartDay = inv.injuryStartDay;
                    return;
                }
            }
            m_injured.push_back(std::move(inv));
        }

        void RemoveInjured(RE::FormID id) {
            std::lock_guard lock(m_mutex);
            std::erase_if(m_injured, [id](const InjuredFollower& i) { return i.formID == id; });
        }

        InjuredFollower* GetInjured(RE::FormID id) {
            std::lock_guard lock(m_mutex);
            for (auto& i : m_injured) {
                if (i.formID == id) return &i;
            }
            return nullptr;
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
            std::lock_guard lock(m_mutex);
            m_dead.clear();
            m_injured.clear();
            m_reviveCounts.clear();
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
                item["bleedoutCount"] = inf.bleedoutCount;
                item["injuryStartDay"] = inf.injuryStartDay;
                item["lastBleedoutDay"] = inf.lastBleedoutDay;
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
                        inf.bleedoutCount = item.value("bleedoutCount", 0);
                        inf.injuryStartDay = item.value("injuryStartDay", 0.0f);
                        inf.lastBleedoutDay = item.value("lastBleedoutDay", 0.0f);
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


    // ---- Yardımcı fonksiyonlar ----

    // ---- Yaralanma Bandaji Yardimci Fonksiyonlari ----
    // ---- Yaralanma Bandaji Yardimci Fonksiyonlari ----
    // "Usable Skyrim Bandages.esp" yukluyse kafa bandajini giydirir/cikarir.
    // Mod yuklu degilse hicbir islem yapilmaz.
    // ESL plugin - yerel FormID 0x800 (ClothesHeadBandages)
    static constexpr std::uint32_t kBandageLocalFormID = 0x800;
    static constexpr const char*   kBandagePlugin      = "Usable Skyrim Bandages.esp";

    inline RE::TESObjectARMO* GetBandageItem() {
        auto* handler = RE::TESDataHandler::GetSingleton();
        if (!handler) return nullptr;
        if (!handler->LookupModByName(kBandagePlugin)) return nullptr;
        return handler->LookupForm<RE::TESObjectARMO>(kBandageLocalFormID, kBandagePlugin);
    }

    inline void EquipBandage(RE::Actor* actor) {
        if (!actor) return;
        auto* bandage = GetBandageItem();
        if (!bandage) return;

        // Moda ait bandaji envantere ekle ve giydir (Circlet/Kafa slotu)
        actor->AddObjectToContainer(bandage, nullptr, 1, nullptr);
        RE::ActorEquipManager::GetSingleton()->EquipObject(actor, bandage, nullptr, 1);
        spdlog::info("LifeAgain: {} kafa bandaji takildi ({}).", actor->GetDisplayFullName(), kBandagePlugin);
    }

    inline void UnequipBandage(RE::Actor* actor) {
        if (!actor) return;
        auto* bandage = GetBandageItem();
        if (!bandage) return;

        // Bandaji cikar ve envanterden sil
        RE::ActorEquipManager::GetSingleton()->UnequipObject(actor, bandage, nullptr, 1);
        actor->RemoveItem(bandage, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        spdlog::info("LifeAgain: {} kafa bandaji cikarildi ({}).", actor->GetDisplayFullName(), kBandagePlugin);
    }

    inline void RemoveInjuryDebuffs(RE::Actor* actor, InjuryLevel level) {
        if (!actor || level == InjuryLevel::None) return;
        auto owner = actor->AsActorValueOwner();
        if (owner) {
            float damageMod = (level == InjuryLevel::Heavy) ? 0.50f : 0.15f;
            float speedMod  = (level == InjuryLevel::Heavy) ? 50.0f : 20.0f;
            float regenMod  = (level == InjuryLevel::Heavy) ? 75.0f : 25.0f;
            float armorMod  = (level == InjuryLevel::Heavy) ? 150.0f : 50.0f;

            owner->ModActorValue(RE::ActorValue::kAttackDamageMult, damageMod);
            owner->ModActorValue(RE::ActorValue::kSpeedMult, speedMod);
            owner->ModActorValue(RE::ActorValue::kHealRateMult, regenMod);
            owner->ModActorValue(RE::ActorValue::kMagickaRateMult, regenMod);
            owner->ModActorValue(RE::ActorValue::kStaminaRateMult, regenMod);
            owner->ModActorValue(RE::ActorValue::kDamageResist, armorMod);
        }

        // Bandaji cikar (mod yukluyse)
        UnequipBandage(actor);
        spdlog::info("LifeAgain: Injury debuffs removed from {}", actor->GetDisplayFullName());
    }


    inline void ApplyInjuryDebuffs(RE::Actor* actor, InjuryLevel oldLevel, InjuryLevel newLevel) {
        if (!actor) return;
        // Önce eskileri geri al (eğer varsa)
        if (oldLevel != InjuryLevel::None) {
            RemoveInjuryDebuffs(actor, oldLevel);
        }
        
        // Yenisini uygula
        if (newLevel != InjuryLevel::None) {
            auto owner = actor->AsActorValueOwner();
            if (owner) {
                float damageMod = (newLevel == InjuryLevel::Heavy) ? -0.50f : -0.15f;
                float speedMod  = (newLevel == InjuryLevel::Heavy) ? -50.0f : -20.0f;
                float regenMod  = (newLevel == InjuryLevel::Heavy) ? -75.0f : -25.0f; // Magicka, Stamina, Health regen
                float armorMod  = (newLevel == InjuryLevel::Heavy) ? -150.0f : -50.0f; // Armor rating

                owner->ModActorValue(RE::ActorValue::kAttackDamageMult, damageMod);
                owner->ModActorValue(RE::ActorValue::kSpeedMult, speedMod);
                owner->ModActorValue(RE::ActorValue::kHealRateMult, regenMod);
                owner->ModActorValue(RE::ActorValue::kMagickaRateMult, regenMod);
                owner->ModActorValue(RE::ActorValue::kStaminaRateMult, regenMod);
                owner->ModActorValue(RE::ActorValue::kDamageResist, armorMod);
            }
            spdlog::info("LifeAgain: {} is now injured (Level: {}). SpeedMod & DamageMod applied.", actor->GetDisplayFullName(), (int)newLevel);
            
            // Bandaj tak (mod yukluyse)
            EquipBandage(actor);

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
            raceEdid.find("cat") != std::string::npos)
            return FollowerKind::Cat;

        return FollowerKind::Human;
    }

    // Aktörün şu an aktif olarak oyuncuyu takip edip etmediğini kontrol et
    inline bool IsFollower(RE::Actor* actor) {
        if (!actor || actor->IsPlayerRef()) return false;

        // 1) IsPlayerTeammate() kontrolü (Vanilla, NFF, EFF, AFT, Inigo, Lucien ve tüm aktif yoldaşlarda oyuncuyu takip ederken true olur)
        if (actor->IsPlayerTeammate()) return true;

        // 2) CurrentFollowerFaction (0x0005C84D) ve rütbe >= 0 kontrolü (sadece aktif takip ederken rütbe >= 0 olur)
        auto* followerFaction = static_cast<RE::TESFaction*>(RE::TESForm::LookupByID(0x5C84D));
        if (followerFaction && actor->GetFactionRank(followerFaction, false) >= 0) return true;

        // 3) Köpek ve evcil hayvan takipçileri (CurrentDogFaction: 0x000DAB74 / CurrentHirelingFaction: 0x000918E2)
        auto* dogFaction = static_cast<RE::TESFaction*>(RE::TESForm::LookupByID(0xDAB74));
        if (dogFaction && actor->GetFactionRank(dogFaction, false) >= 0) return true;

        auto* hirelingFaction = static_cast<RE::TESFaction*>(RE::TESForm::LookupByID(0x918E2));
        if (hirelingFaction && actor->GetFactionRank(hirelingFaction, false) >= 0) return true;

        return false;
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
                lower.find("mara") != std::string::npos ||
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

            spdlog::info("LifeAgain: Follower died -> {} (Level {}) at {}", df.name, df.level, df.deathDate);
            FollowerTracker::GetSingleton().AddDead(std::move(df));

            std::string msg = "Your follower " + df.name + " has died! (Yoldasiniz " + df.name + " oldu!)";
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
            InjuredFollower* inf = tracker->GetInjured(formID);

            int chance = g_Settings.BleedoutDeathChance_Healthy;
            if (inf) {
                if (inf->injuryLevel == InjuryLevel::Light) chance = g_Settings.BleedoutDeathChance_Light;
                else if (inf->injuryLevel == InjuryLevel::Heavy) chance = g_Settings.BleedoutDeathChance_Heavy;
            }

            if (chance <= 0) return RE::BSEventNotifyControl::kContinue;

            static std::mt19937 rng{ std::random_device{}() };
            std::uniform_int_distribution<int> dist(1, 100);
            
            if (dist(rng) <= chance) {
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
            } else {
                // Ölmedi, yaralanma ihtimalini kontrol et
                std::uniform_int_distribution<int> injDist(1, 100);
                if (injDist(rng) <= g_Settings.InjuryChance) {
                    InjuredFollower newInf;
                    InjuryLevel oldLevel = InjuryLevel::None;

                    // lastBleedoutDay güncelle
                    auto* cal = RE::Calendar::GetSingleton();
                    float currentDay = cal ? cal->GetDaysPassed() : 0.0f;

                    if (inf) {
                        newInf = *inf; // Kopyala
                        oldLevel = inf->injuryLevel;
                        newInf.lastBleedoutDay = currentDay;
                        if (inf->injuryLevel == InjuryLevel::Light) {
                            newInf.bleedoutCount++;
                            if (newInf.bleedoutCount >= 3) {
                                newInf.injuryLevel = InjuryLevel::Heavy;
                            }
                        }
                    } else {
                        newInf.formID = formID;
                        newInf.name = actor->GetDisplayFullName();
                        newInf.level = actor->GetLevel();
                        newInf.kind = DetectKind(actor);
                        newInf.injuryLevel = InjuryLevel::Light;
                        newInf.bleedoutCount = 0;
                        newInf.injuryStartDay = currentDay;
                        newInf.lastBleedoutDay = currentDay;
                    }

                    tracker->AddOrUpdateInjured(newInf);

                    // ModActorValue / PlaySound / NotifyAnimationGraph çağrılarını
                    // bleedout event handler'ından DOĞRUDAN yapmak yerine AddTask'a taşı.
                    // Bu sayede SKSE overlay task'larıyla (skee64, DynamicArmorVariants) çakışmaz.
                    auto handle = actor->GetHandle();
                    InjuryLevel capturedOld = oldLevel;
                    InjuryLevel capturedNew = newInf.injuryLevel;
                    bool isFemale = false;
                    if (auto* base = actor->GetActorBase()) {
                        isFemale = (base->GetSex() == 1);
                    }

                    if (auto* taskInterface = SKSE::GetTaskInterface()) {
                        taskInterface->AddTask([handle, capturedOld, capturedNew, isFemale]() {
                            auto actorPtr = handle.get();
                            if (!actorPtr) return;
                            auto* a = actorPtr.get();
                            if (!a || a->IsDead()) return;

                            if (capturedOld != capturedNew) {
                                ApplyInjuryDebuffs(a, capturedOld, capturedNew);
                            } else {
                                // Sadece inleme sesi
                                if (isFemale) {
                                    RE::PlaySound("NPCHumanFemaleGroan");
                                } else {
                                    RE::PlaySound("NPCHumanMaleGroan");
                                }
                            }
                        });
                    }
                }
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    inline void CheckPotionHealing(RE::Actor* actor, InjuredFollower* inf) {
        if (!actor || actor->IsDead() || !inf) return;

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
        int needed = (inf->injuryLevel == InjuryLevel::Heavy) ? g_Settings.PotionRequirementHeavy : g_Settings.PotionRequirementLight;

        if (potionCount >= needed) {
            actor->RemoveItem(potionForm, needed, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            RemoveInjuryDebuffs(actor, inf->injuryLevel);
            
            std::string name = inf->name;
            InjuryLevel level = inf->injuryLevel;
            
            FollowerTracker::GetSingleton().RemoveInjured(inf->formID);

            std::string msg;
            if (level == InjuryLevel::Light) {
                msg = fmt::format("{} used {} Cure Disease Potions and healed their light injury! ({} {} ilac kullanarak hafif yarasini iyilestirdi!)", name, needed, name, needed);
            } else {
                msg = fmt::format("{} used {} Cure Disease Potions and healed their heavy injury! ({} {} ilac kullanarak agir yarasini iyilestirdi!)", name, needed, name, needed);
            }
            RE::DebugNotification(msg.c_str());
            spdlog::info("LifeAgain: {} healed via potion.", name);
        }
    }

    inline void CheckNaturalHealing() {
        auto* cal = RE::Calendar::GetSingleton();
        if (!cal) return;
        float today = cal->GetDaysPassed();

        auto& tracker = FollowerTracker::GetSingleton();
        auto injured = tracker.GetInjuredFollowers();

        for (auto& inf : injured) {
            if (inf.injuryLevel != InjuryLevel::Light) continue;

            float daysSinceBleedout = today - inf.lastBleedoutDay;
            if (daysSinceBleedout >= 3.0f) {
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(inf.formID);
                if (actor && !actor->IsDead()) {
                    RemoveInjuryDebuffs(actor, InjuryLevel::Light);
                    
                    std::string msg = fmt::format("{} has recovered naturally. ({} sagligina kavustu.)", inf.name, inf.name);
                    RE::DebugNotification(msg.c_str());
                    spdlog::info("LifeAgain: {} naturally healed after {:.1f} days.", inf.name, daysSinceBleedout);
                }
                tracker.RemoveInjured(inf.formID); // Even if actor is unloaded, remove from list
            }
        }
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
            InjuredFollower* inf = tracker->GetInjured(event->newContainer);
            if (inf) {
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(event->newContainer);
                if (actor) {
                    // Cagiriyi asenkron yapalim ki envanter tam guncellenmis olsun
                    RE::FormID formID = event->newContainer;
                    if (auto* task = SKSE::GetTaskInterface()) {
                        task->AddTask([formID]() {
                            auto* a = RE::TESForm::LookupByID<RE::Actor>(formID);
                            auto* tr = &FollowerTracker::GetSingleton();
                            InjuredFollower* i = tr->GetInjured(formID);
                            if (a && i) CheckPotionHealing(a, i);
                        });
                    }
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    // ---- Wait / Sleep Event (Zamanli Iyilesme) ----
    class WaitStopEventSink : public RE::BSTEventSink<RE::TESWaitStopEvent> {
    public:
        static WaitStopEventSink* GetSingleton() { static WaitStopEventSink instance; return &instance; }
        RE::BSEventNotifyControl ProcessEvent(const RE::TESWaitStopEvent*, RE::BSTEventSource<RE::TESWaitStopEvent>*) override {
            CheckNaturalHealing();
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    class SleepStopEventSink : public RE::BSTEventSink<RE::TESSleepStopEvent> {
    public:
        static SleepStopEventSink* GetSingleton() { static SleepStopEventSink instance; return &instance; }
        RE::BSEventNotifyControl ProcessEvent(const RE::TESSleepStopEvent*, RE::BSTEventSource<RE::TESSleepStopEvent>*) override {
            CheckNaturalHealing();
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    class FastTravelEndEventSink : public RE::BSTEventSink<RE::TESFastTravelEndEvent> {
    public:
        static FastTravelEndEventSink* GetSingleton() { static FastTravelEndEventSink instance; return &instance; }
        RE::BSEventNotifyControl ProcessEvent(const RE::TESFastTravelEndEvent*, RE::BSTEventSource<RE::TESFastTravelEndEvent>*) override {
            CheckNaturalHealing();
            return RE::BSEventNotifyControl::kContinue;
        }
    };


    inline void RegisterDeathEventSink() {
        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (holder) {
            holder->AddEventSink(DeathEventSink::GetSingleton());
            holder->AddEventSink(ActivateEventSink::GetSingleton());
            holder->AddEventSink(BleedoutEventSink::GetSingleton());
            holder->AddEventSink(ContainerChangedEventSink::GetSingleton());
            holder->AddEventSink(WaitStopEventSink::GetSingleton());
            holder->AddEventSink(SleepStopEventSink::GetSingleton());
            holder->AddEventSink(FastTravelEndEventSink::GetSingleton());
            spdlog::info("LifeAgain: Event Sinks registered successfully.");
        }
    }
}

