#pragma once
#include <SKSE/SKSE.h>
#include <RE/I/InputEvent.h>
#include <RE/B/ButtonEvent.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/ProcessLists.h>
#include <RE/T/TESBoundObject.h>
#include <RE/B/BSInputDeviceManager.h>
#include <SKSEMenuFramework.h>
#include <spdlog/spdlog.h>
#include <RE/I/InventoryEntryData.h>
#include <RE/I/InventoryChanges.h>
#include <RE/E/ExtraDataList.h>
#include <RE/S/SoulLevels.h>
#include <RE/F/FormTraits.h>  // TESForm::As<T> tanımı burada (TESForm.h yalnızca bildirimi içerir)
#include "FollowerTracker.h"
#include "Settings.h"
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>

namespace LifeAgain {

    struct SoulGemInfo {
        RE::FormID id;
        int requiredAmount;
        std::string name;
    };

    inline std::vector<SoulGemInfo> GetSoulGemHierarchy() {
        return {
            { 0x0002E504, 1, "Black Soul Gem (Siyah Ruh Tasi)" },
            { 0x0002E4FF, 1, "Grand Soul Gem (Yuce Ruh Tasi)" },
            { 0x0002E4FB, 2, "Greater Soul Gem (Buyuk Ruh Tasi)" },
            { 0x0002E4F3, 3, "Common Soul Gem (Yaygin Ruh Tasi)" },
            { 0x0002E4E5, 4, "Lesser Soul Gem (Orta Ruh Tasi)" },
            { 0x0002E4E3, 5, "Petty Soul Gem (Kucuk Ruh Tasi)" }
        };
    }

    // Belirtilen formdaki DOLU ruh taşlarının sayısını döndürür.
    // GetInventory() aynı formun bos/dolu kayıtlarını tek girdide birleştirdiği
    // için InventoryChanges bağlı listesi doğrudan gezilir ve ExtraSoul kontrolü yapılır.
    inline int CountFilledSoulGems(RE::FormID a_formID) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return 0;
        auto* form = RE::TESForm::LookupByID(a_formID);
        if (!form) return 0;
        auto* boundObj = static_cast<RE::TESBoundObject*>(form);
        if (!boundObj) return 0;

        int count = 0;
        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList) return 0;
        for (auto& entry : *invChanges->entryList) {
            if (entry && entry->object == boundObj && entry->countDelta > 0 &&
                entry->GetSoulLevel() != RE::SOUL_LEVEL::kNone) {
                count += entry->countDelta;
            }
        }
        return count;
    }

    inline bool GetAvailableSoulGem(RE::FormID& outFormID, int& outCount, std::string& outName) {
        for (const auto& gem : GetSoulGemHierarchy()) {
            if (CountFilledSoulGems(gem.id) >= gem.requiredAmount) {
                outFormID = gem.id;
                outCount = gem.requiredAmount;
                outName = gem.name;
                return true;
            }
        }
        return false;
    }

    // Yalnızca DOLU taşları düşer; boş taşlara dokunmaz.
    inline void RemoveSoulGem(RE::FormID formID, int count) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;
        auto* form = RE::TESForm::LookupByID(formID);
        if (!form) return;
        auto* boundObj = static_cast<RE::TESBoundObject*>(form);
        if (!boundObj) return;

        // Envanter değişikliği liste girdilerini bozabileceği için önce snapshot al
        struct FilledEntry { RE::ExtraDataList* extra; std::int32_t count; };
        std::vector<FilledEntry> filled;
        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList) return;
        for (auto& entry : *invChanges->entryList) {
            if (entry && entry->object == boundObj && entry->countDelta > 0 &&
                entry->GetSoulLevel() != RE::SOUL_LEVEL::kNone) {
                RE::ExtraDataList* extra = nullptr;
                if (entry->extraLists) {
                    for (auto& xList : *entry->extraLists) {
                        if (xList && xList->GetSoulLevel() != RE::SOUL_LEVEL::kNone) {
                            extra = xList;
                            break;
                        }
                    }
                }
                filled.push_back({ extra, entry->countDelta });
            }
        }

        int remaining = count;
        for (const auto& e : filled) {
            if (remaining <= 0) break;
            // windows.h min makrosuyla çakışmasın diye üçlü operatör
            const int take = remaining < static_cast<int>(e.count) ? remaining : static_cast<int>(e.count);
            player->RemoveItem(boundObj, take, RE::ITEM_REMOVE_REASON::kRemove, e.extra, nullptr);
            remaining -= take;
        }
    }

    inline SKSEMenuFramework::Model::WindowInterface* g_Window = nullptr;

    inline int GetPlayerCurePotionCount() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return 0;
        auto* potion = RE::TESForm::LookupByID(0x000AE723);
        if (!potion || potion->GetFormType() != RE::FormType::AlchemyItem) return 0;
        return player->GetItemCount(static_cast<RE::TESBoundObject*>(potion));
    }

    inline int GetPlayerGold() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* gold = static_cast<RE::TESBoundObject*>(RE::TESForm::LookupByID(0x0000000F));
        return player && gold ? player->GetItemCount(gold) : 0;
    }

    inline bool RemovePlayerGold(int amount) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* gold = static_cast<RE::TESBoundObject*>(RE::TESForm::LookupByID(0x0000000F));
        if (!player || !gold || player->GetItemCount(gold) < amount) return false;
        player->RemoveItem(gold, amount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        return true;
    }

    inline int GetActorGold(RE::Actor* actor) {
        return GetSafeActorGoldCount(actor);
    }

    inline bool RemoveActorGold(RE::Actor* actor, int amount) {
        auto* gold = static_cast<RE::TESBoundObject*>(RE::TESForm::LookupByID(0x0000000F));
        if (!actor || !gold || GetSafeActorGoldCount(actor) < amount) return false;
        actor->RemoveItem(gold, amount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        return true;
    }

    inline bool RemovePlayerCurePotions(int amount) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* potion = RE::TESForm::LookupByID(0x000AE723);
        if (!player || !potion || potion->GetFormType() != RE::FormType::AlchemyItem) return false;
        auto* bound = static_cast<RE::TESBoundObject*>(potion);
        if (player->GetItemCount(bound) < amount) return false;
        player->RemoveItem(bound, amount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        return true;
    }

    inline std::string DescribeWounds(const InjuredFollower& follower) {
        std::string result;
        for (std::size_t i = 0; i < follower.injuryCounts.size(); ++i) {
            if (!follower.injuryCounts[i]) continue;
            // Separate wound types so narrow menu columns cannot hide later wounds.
            if (!result.empty()) result += "\n";
            result += std::to_string(follower.injuryCounts[i]) + " x " + kInjuryTypeNames[i];
        }
        return result.empty() ? "None" : result;
    }

    inline std::string NaturalHealingStatus(const InjuredFollower& follower) {
        const auto lightWounds = std::accumulate(follower.injuryCounts.begin(), follower.injuryCounts.begin() + 4, 0u);
        if (lightWounds == 0) return "No light wounds (Hafif yara yok)";

        constexpr float recoveryDaysPerLightWound = 3.0f;
        float progressDays = follower.recoveryProgressDays;
        if (auto* calendar = RE::Calendar::GetSingleton(); calendar && follower.recoveryLastGameDay >= 0.0f) {
            progressDays += std::max(0.0f, calendar->GetDaysPassed() - follower.recoveryLastGameDay);
        }
        progressDays = std::fmod(std::max(0.0f, progressDays), recoveryDaysPerLightWound);
        const float remainingDays = recoveryDaysPerLightWound - progressDays;
        return fmt::format("Next light wound: {:.1f} game days (Sonraki hafif yara: {:.1f} oyun gunu)", remainingDays, remainingDays);
    }

    inline bool ReviveFollower(RE::FormID formID) {
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
        if (!actor) return false;
        if (actor->IsDead()) {
            actor->Resurrect(false, false);
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            actor->MoveTo(player->AsReference());
        }

        // Dirilen yoldasta kalabilecek debuff kalintilari temizle.
        // KillImmediate + Resurrect sonrasi ModActorValue degerleri bazen geri gelmez,
        // guvenli olarak her iki seviye icin de ters duzeltme uygula.
        auto* tracker = &FollowerTracker::GetSingleton();
        auto inf = tracker->GetInjured(formID);
        if (inf) {
            // savedSpeedBase ile tam restore + IdleWoundedStop animasyon sifirlama
            RemoveInjuryDebuffs(actor, &*inf);
            tracker->RemoveInjured(formID);
        }

        FollowerTracker::GetSingleton().AddReviveCount(formID);
        spdlog::info("LifeAgain: {} hayata donduruldu", actor->GetDisplayFullName());
        return true;
    }


    inline const char* KindLabel(FollowerKind k) {
        switch (k) {
            case FollowerKind::Dog:   return "[Dog (Kopek)]";
            case FollowerKind::Cat:   return "[Cat (Kedi)] ";
            case FollowerKind::Horse: return "[Horse (At)] ";
            default:                  return "[Human (Insan)]";
        }
    }

    struct MenuState {
        std::vector<DeadFollower> list;
        std::vector<InjuredFollower> injuredList;
        int  selectedIndex   = -1;
        int  selectedInjuredIndex = -1;
        bool confirmVisible  = false;
        bool confirmHealVisible = false;
        bool confirmHealWithGold = false;
        int  confirmCost     = 0;
        int  confirmHealCost = 0;
        RE::FormID selectedTestActor = 0;
        int selectedTestInjury = 0;
        int testItemAmount = 1;
        int testNpcGoldAmount = 500;
    };

    struct TestFollowerSnapshot {
        RE::FormID formID = 0;
        std::string name;
        int level = 1;
        std::optional<InjuredFollower> injury;
    };

    inline MenuState  g_MenuState;
    inline std::mutex g_MenuMutex;
    inline std::mutex g_TestFollowerMutex;
    inline std::vector<TestFollowerSnapshot> g_TestFollowers;
    inline std::atomic<bool> g_TestRefreshPending{ false };

    inline void RefreshList() {
        std::lock_guard lock(g_MenuMutex);
        
        auto newList = FollowerTracker::GetSingleton().GetDeadFollowers();
        auto rawInjuredList = FollowerTracker::GetSingleton().GetInjuredFollowers();
        
        // Ölü olan hiçbir takipçi yaralı listesinde yer alamaz
        std::vector<InjuredFollower> newInjuredList;
        for (auto& inf : rawInjuredList) {
            bool isDead = false;
            for (auto& df : newList) {
                if (df.formID == inf.formID) {
                    isDead = true;
                    break;
                }
            }
            if (!isDead) {
                newInjuredList.push_back(inf);
            }
        }
        
        // Eger onceden secili biri varsa formID'sini hatirla
        RE::FormID selectedID = 0;
        if (g_MenuState.selectedIndex >= 0 && g_MenuState.selectedIndex < (int)g_MenuState.list.size()) {
            selectedID = g_MenuState.list[g_MenuState.selectedIndex].formID;
        }

        RE::FormID selectedInjuredID = 0;
        if (g_MenuState.selectedInjuredIndex >= 0 && g_MenuState.selectedInjuredIndex < (int)g_MenuState.injuredList.size()) {
            selectedInjuredID = g_MenuState.injuredList[g_MenuState.selectedInjuredIndex].formID;
        }

        g_MenuState.list           = std::move(newList);
        g_MenuState.injuredList    = std::move(newInjuredList);
        g_MenuState.selectedIndex  = -1;
        g_MenuState.selectedInjuredIndex = -1;
        g_MenuState.confirmVisible = false;
        g_MenuState.confirmHealVisible = false;

        // Eski secileni tekrar bul
        if (selectedID != 0) {
            for (int i = 0; i < (int)g_MenuState.list.size(); ++i) {
                if (g_MenuState.list[i].formID == selectedID) {
                    g_MenuState.selectedIndex = i;
                    break;
                }
            }
        }
        
        if (selectedInjuredID != 0) {
            for (int i = 0; i < (int)g_MenuState.injuredList.size(); ++i) {
                if (g_MenuState.injuredList[i].formID == selectedInjuredID) {
                    g_MenuState.selectedInjuredIndex = i;
                    break;
                }
            }
        }
    }

    inline void TextColored4(float r, float g, float b, float a, const char* fmt, ...) {
        using namespace ImGuiMCP;
        ImVec4 col{r, g, b, a};
        char buf[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        TextColored(col, "%s", buf);
    }

    inline std::vector<TestFollowerSnapshot> GetTestFollowerSnapshot()
    {
        std::lock_guard lock(g_TestFollowerMutex);
        return g_TestFollowers;
    }

    inline void RequestTestFollowerRefresh()
    {
        bool expected = false;
        if (!g_TestRefreshPending.compare_exchange_strong(expected, true)) return;
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            g_TestRefreshPending = false;
            return;
        }
        tasks->AddTask([]() {
            std::vector<std::pair<float, TestFollowerSnapshot>> candidates;
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* lists = RE::ProcessLists::GetSingleton();
            if (player && lists) {
                lists->ForAllActors([&](RE::Actor& actor) {
                    if (&actor == player || actor.IsDead() ||
                        actor.GetParentCell() != player->GetParentCell() || !IsInjuryEligibleFollower(&actor)) {
                        return RE::BSContainer::ForEachResult::kContinue;
                    }
                    const float distance = actor.GetPosition().GetDistance(player->GetPosition());
                    if (distance > 2500.0f) return RE::BSContainer::ForEachResult::kContinue;
                    TestFollowerSnapshot snapshot;
                    snapshot.formID = actor.GetFormID();
                    snapshot.name = actor.GetDisplayFullName();
                    snapshot.level = actor.GetLevel();
                    snapshot.injury = FollowerTracker::GetSingleton().GetInjured(snapshot.formID);
                    candidates.push_back({ distance, std::move(snapshot) });
                    return RE::BSContainer::ForEachResult::kContinue;
                });
            }
            std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
                return left.first < right.first;
            });
            std::vector<TestFollowerSnapshot> refreshed;
            const auto count = std::min<std::size_t>(candidates.size(), 30);
            refreshed.reserve(count);
            for (std::size_t i = 0; i < count; ++i) refreshed.push_back(std::move(candidates[i].second));
            {
                std::lock_guard lock(g_TestFollowerMutex);
                g_TestFollowers = std::move(refreshed);
            }
            g_TestRefreshPending = false;
        });
    }

    template <class Action>
    inline void QueueTestFollowerAction(RE::FormID formID, Action action)
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        tasks->AddTask([formID, action = std::move(action)]() mutable {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
            if (!IsInjuryEligibleFollower(actor) || actor->IsDead()) return;
            action(actor);
            RequestTestFollowerRefresh();
        });
    }

    inline void __stdcall RenderInjuryTestMenu()
    {
        using namespace ImGuiMCP;
        auto& state = g_MenuState;
        static int lastRefreshFrame = -120;
        const int frame = ImGuiMCP::GetFrameCount();
        if (frame - lastRefreshFrame >= 120) {
            RequestTestFollowerRefresh();
            lastRefreshFrame = frame;
        }
        const auto followers = GetTestFollowerSnapshot();

        TextColored4(1.0f, 0.85f, 0.3f, 1.0f, "--- INJURY TEST LAB (YARALANMA TESTI) ---");
        Text("Only nearby active humanoid followers are listed. Test changes are saved with the current save.");
        Text("Yalnizca yakindaki aktif insansi yoldaslar listelenir. Test degisiklikleri kayda yazilir.");
        Spacing();

        if (Button("  Refresh nearby followers (Yakin yoldaslari yenile)  ")) RequestTestFollowerRefresh();
        if (followers.empty()) {
            Text("No active humanoid followers found within 2500 units in this cell.");
            return;
        }

        Text("Choose a follower (Yoldas secin):");
        for (const auto& follower : followers) {
            const std::string label = follower.name + "##test_" + std::to_string(follower.formID);
            if (Selectable(label.c_str(), state.selectedTestActor == follower.formID)) state.selectedTestActor = follower.formID;
        }
        if (state.selectedTestActor == 0) return;
        const auto selected = std::find_if(followers.begin(), followers.end(), [&](const auto& entry) {
            return entry.formID == state.selectedTestActor;
        });
        if (selected == followers.end()) {
            state.selectedTestActor = 0;
            return;
        }
        Separator();
        Text("Selected follower (Secili yoldas): %s | Level (Seviye): %d",
            selected->name.c_str(), selected->level);
        if (const auto& injury = selected->injury) {
            Text("Injury (Yaralanma): %s | Potion cost: %d | Gold cost: %d",
                injury->injuryLevel == InjuryLevel::Heavy ? "Heavy (Agir)" : "Light (Hafif)",
                GetInjuryPotionCost(injury->injuryCounts), GetInjuryGoldCost(injury->injuryCounts));
            Text("Wounds (Yaralar):");
            TextWrapped("%s", DescribeWounds(*injury).c_str());
        } else {
            Text("Injury (Yaralanma): None");
        }

        if (SliderInt("Wound type (Yara turu)", &state.selectedTestInjury, 0, static_cast<int>(kInjuryTypeCount) - 1)) {}
        Text("Selected wound: %s", kInjuryTypeNames[static_cast<std::size_t>(state.selectedTestInjury)]);
        if (Button("  Apply selected wound (Secili yarayi uygula)  ")) {
            const auto type = static_cast<InjuryType>(state.selectedTestInjury);
            QueueTestFollowerAction(state.selectedTestActor, [type](RE::Actor* actor) {
                RecordBattleWound(actor, type);
            });
        }

        SliderInt("Cure potions to give (Verilecek iksir)", &state.testItemAmount, 1, 20);
        if (Button("  Give Cure Disease Potion(s) (Yoldasa tedavi iksiri ver)  ")) {
            const int amount = state.testItemAmount;
            QueueTestFollowerAction(state.selectedTestActor, [amount](RE::Actor* actor) {
                auto* potion = RE::TESForm::LookupByID(0x000AE723);
                if (potion && potion->GetFormType() == RE::FormType::AlchemyItem)
                    actor->AddObjectToContainer(static_cast<RE::TESBoundObject*>(potion), nullptr, amount, nullptr);
            });
        }
        SliderInt("Gold to give (Verilecek altin)", &state.testNpcGoldAmount, 100, 10000);
        if (Button("  Give gold to follower (Yoldasa altin ver)  ")) {
            const int amount = state.testNpcGoldAmount;
            QueueTestFollowerAction(state.selectedTestActor, [amount](RE::Actor* actor) {
                auto* gold = static_cast<RE::TESBoundObject*>(RE::TESForm::LookupByID(0x0000000F));
                if (gold) actor->AddObjectToContainer(gold, nullptr, amount, nullptr);
            });
        }

        if (Button("  Test carried-potion treatment (NPC iksiriyle tedavi et)  ")) {
            QueueTestFollowerAction(state.selectedTestActor, [](RE::Actor* actor) {
                auto injury = FollowerTracker::GetSingleton().GetInjured(actor->GetFormID());
                if (injury) CheckPotionHealing(actor, &*injury);
            });
        }
        SameLine();
        if (Button("  Test carried-gold treatment (NPC altiniyla tedavi et)  ")) {
            QueueTestFollowerAction(state.selectedTestActor, [](RE::Actor* actor) {
                auto& tracker = FollowerTracker::GetSingleton();
                auto injury = tracker.GetInjured(actor->GetFormID());
                if (!injury) return;
                const int cost = GetInjuryGoldCost(injury->injuryCounts);
                if (RemoveActorGold(actor, cost)) {
                    RemoveInjuryDebuffs(actor, &*injury);
                    tracker.RemoveInjured(actor->GetFormID());
                }
            });
        }
        if (Button("  Clear test injury for this NPC (Test yarasini temizle)  ")) {
            QueueTestFollowerAction(state.selectedTestActor, [](RE::Actor* actor) {
                auto& tracker = FollowerTracker::GetSingleton();
                auto injury = tracker.GetInjured(actor->GetFormID());
                if (!injury) return;
                RemoveInjuryDebuffs(actor, &*injury);
                tracker.RemoveInjured(actor->GetFormID());
            });
        }
    }

    // ============ SKSE MENU FRAMEWORK ARAYÜZÜ ============
    inline void __stdcall RenderSettingsMenu() {
        using namespace ImGuiMCP;
        
        // --- AYARLAR BÖLÜMÜ ---
        TextColored4(1.0f, 0.85f, 0.3f, 1.0f, "--- SETTINGS (AYARLAR) ---");
        if (Checkbox("Use Gold For Revive (Diriltme Icin Altin Kullan)", &g_Settings.UseGoldForRevive)) {
            SaveSettings();
        }
        if (Checkbox("Use Soul Gems For Revive (Diriltme Icin Ruh Tasi Kullan)", &g_Settings.UseSoulGemsForRevive)) {
            SaveSettings();
        }
        Spacing();

        if (SliderInt("Revive Cost Multiplier (Diriltme Ucret Carpani)", &g_Settings.GoldMultiplier, 50, 5000)) {
            SaveSettings();
        }
        Text("Per level: %d gold (Her 1 seviye basina: %d altin)", g_Settings.GoldMultiplier, g_Settings.GoldMultiplier);
        Spacing();

        Text("Wounds can be treated manually from the Healing menu or with Cure Disease Potions.");
        Spacing();

        if (SliderInt("Healthy Death Chance % (Normal Olme Sansi)", &g_Settings.BleedoutDeathChance_Healthy, 0, 100)) {
            SaveSettings();
        }
        if (SliderInt("Light Injury Death Chance % (Hafif Yarali Olme Sansi)", &g_Settings.BleedoutDeathChance_Light, 0, 100)) {
            SaveSettings();
        }
        if (SliderInt("Heavy Injury Death Chance % (Agir Yarali Olme Sansi)", &g_Settings.BleedoutDeathChance_Heavy, 0, 100)) {
            SaveSettings();
        }
        Text("0 = Vanilla rules (Sadece normal oyun kurallari).");
        Spacing();

        if (Checkbox("Show Death Notification (Olum Bildirimi Goster)", &g_Settings.ShowDeathNotification)) {
            SaveSettings();
        }
        if (Checkbox("Show Death Message Box (Olum Mesaj Kutusu Goster)", &g_Settings.ShowDeathMessageBox)) {
            SaveSettings();
        }
        Spacing();
        
        if (SliderInt("Light Injury Risk % (Hafif Yaralanma Riski)", &g_Settings.LightInjuryChance, 0, 100)) {
            const int maxHeavyRisk = g_Settings.LightInjuryChance > 1 ? (g_Settings.LightInjuryChance - 1) / 2 : 0;
            g_Settings.HeavyInjuryChance = std::min(g_Settings.HeavyInjuryChance, maxHeavyRisk);
            SaveSettings();
        }
        const int maxHeavyRisk = g_Settings.LightInjuryChance > 1 ? (g_Settings.LightInjuryChance - 1) / 2 : 0;
        if (SliderInt("Heavy Injury Risk % (Agir Yaralanma Riski)", &g_Settings.HeavyInjuryChance, 0, maxHeavyRisk)) {
            SaveSettings();
        }
        Text("Defaults: light 5%%, heavy 1%%. Heavy risk is capped below half the light risk; one roll per follower every 30 seconds.");
        Text("Varsayilan: hafif %%5, agir %%1. Agir risk hafif riskin yarisindan azdir; her takipci icin 30 saniyede bir kontrol edilir.");
        if (Checkbox("Show Injury Notification (Yaralanma Bildirimi Goster)", &g_Settings.ShowInjuryNotification)) {
            SaveSettings();
        }
        if (Checkbox("Show Injury Message Box (Yaralanma Mesaj Kutusu Goster)", &g_Settings.ShowInjuryMessageBox)) {
            SaveSettings();
        }
        Spacing();
        
        if (SliderInt("Max Revives (Max Diriltme Sayisi)", &g_Settings.MaxRevives, 0, 20)) {
            SaveSettings();
        }
        Text("0 = Unlimited Revives (Sinirsiz Diriltme)");
        Spacing();
        
        Text("Manual full treatment: light = 1 potion or 100 gold; heavy = 3 potions or 250 gold.");
        Text("Elle tam tedavi: hafif 1 iksir veya 100 altin; agir 3 iksir veya 250 altin.");
        Spacing();
        
        if (Button("  Save Settings (Ayarlari Kaydet)  ")) {
            SaveSettings();
        }
    }

    inline void __stdcall RenderRevivalMenu() {
        using namespace ImGuiMCP;
        auto& state = g_MenuState;
        
        static int s_lastFrame = 0;
        int currentFrame = ImGuiMCP::GetFrameCount();
        if (currentFrame - s_lastFrame > 1) {
            RefreshList();
        }
        s_lastFrame = currentFrame;

        // --- DİRİLTME BÖLÜMÜ ---
        TextColored4(1.0f, 0.85f, 0.3f, 1.0f, "--- REVIVAL MENU (DIRILTME MENUSU) ---");
        Spacing();

        if (!g_PriestAccess.load()) {
            TextColored4(1.0f, 0.45f, 0.2f, 1.0f,
                "This service is only provided by a priest or monk. (Bu hizmet yalnizca bir rahip veya kesis tarafindan sunulur.)");
            TextColored4(0.65f, 0.65f, 0.65f, 1.0f,
                "Find a religious figure and speak to them. (Bir din adami bulun ve onunla konusun.)");
        } else {
            TextColored4(0.5f, 1.0f, 0.5f, 1.0f,
                "%s is listening to you... (%s sizi dinliyor...)", g_PriestName.c_str(), g_PriestName.c_str());
            
            Spacing();

            if (state.list.empty()) {
                TextColored4(0.7f, 0.7f, 0.7f, 1.0f,
                    "You have no dead followers at the moment. (Su an icin olen hicbir takipciniz bulunmamaktadir.)");
            } else {
                Text("Total dead followers recorded (Toplam olen yoldas): %d", (int)state.list.size());
                Spacing();

                constexpr int tableFlags =
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

                if (BeginTable("##FollowerTable", 4, tableFlags, ImVec2{0.0f, 200.0f})) {
                    TableSetupColumn("Type (Tur)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    TableSetupColumn("Name (Ad)", ImGuiTableColumnFlags_WidthStretch, 0.0f);
                    TableSetupColumn("Level (Seviye)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    TableSetupColumn("Date (Tarih)", ImGuiTableColumnFlags_WidthFixed, 130.0f);
                    TableHeadersRow();

                    for (int i = 0; i < (int)state.list.size(); ++i) {
                        const auto& df = state.list[i];
                        bool selected = (state.selectedIndex == i);

                        TableNextRow();
                        TableSetColumnIndex(0);
                        Text("%s", KindLabel(df.kind));

                        TableSetColumnIndex(1);
                        char selId[64];
                        snprintf(selId, sizeof(selId), "%s##row%d", df.name.c_str(), i);
                        if (Selectable(selId, selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                            state.selectedIndex = i;
                            state.confirmVisible = false;
                            state.confirmCost = df.level * g_Settings.GoldMultiplier;
                        }
                        TableSetColumnIndex(2); Text("%d", df.level);
                        TableSetColumnIndex(3); TextUnformatted(df.deathDate.c_str());
                    }
                    EndTable();
                }

                Spacing();
                if (state.selectedIndex >= 0 && state.selectedIndex < (int)state.list.size()) {
                    const auto& sel = state.list[state.selectedIndex];
                    int cost = sel.level * g_Settings.GoldMultiplier;
                    int reviveCount = FollowerTracker::GetSingleton().GetReviveCount(sel.formID);
                    bool reviveLimitReached = (g_Settings.MaxRevives > 0 && reviveCount >= g_Settings.MaxRevives);

                    Separator();
                    TextColored4(1.0f, 0.9f, 0.5f, 1.0f,
                        "Selected (Secili): %s  |  Level (Seviye): %d  |  Cost (Maliyet): %d Gold",
                        sel.name.c_str(), sel.level, cost);
                    TextColored4(0.8f, 0.8f, 0.8f, 1.0f, "Revives used (Diriltme Sayisi): %d", reviveCount);
                    Spacing();

                    if (reviveLimitReached) {
                        TextColored4(1.0f, 0.2f, 0.2f, 1.0f, 
                            "This follower's soul cannot be reached. (Takipcinizin ruhuna erisilemiyor.)");
                        TextColored4(1.0f, 0.4f, 0.4f, 1.0f, 
                            "Maximum revive limit reached. (Maksimum diriltme sinirina ulasildi.)");
                    } else if (!state.confirmVisible) {
                        if (Button("  Revive (Dirilt)  ")) {
                            state.confirmVisible = true;
                            state.confirmCost = cost;
                        }
                    } else {
                        int playerGold = GetPlayerGold();
                        bool hasGold = (playerGold >= state.confirmCost);
                        
                        RE::FormID bestGemId = 0;
                        int bestGemCount = 0;
                        std::string bestGemName;
                        bool hasSoulGem = GetAvailableSoulGem(bestGemId, bestGemCount, bestGemName);
                        
                        bool requireGold = g_Settings.UseGoldForRevive;
                        bool requireSoulGem = g_Settings.UseSoulGemsForRevive;
                        
                        bool canAfford = true;
                        if (requireGold && !hasGold) canAfford = false;
                        if (requireSoulGem && !hasSoulGem) canAfford = false;

                        if (!requireGold && !requireSoulGem) {
                            TextColored4(1.0f, 0.5f, 0.3f, 1.0f, "Reviving %s is free. Are you sure?", sel.name.c_str());
                        } else {
                            TextColored4(1.0f, 0.5f, 0.3f, 1.0f, "Cost for reviving %s (Diriltme bedeli):", sel.name.c_str());
                            if (requireGold) {
                                Text("Gold (Altin): %d / %d", playerGold, state.confirmCost);
                            }
                            if (requireSoulGem) {
                                if (hasSoulGem) {
                                    Text("Soul Gem (Ruh Tasi): %d %s (Found/Bulundu)", bestGemCount, bestGemName.c_str());
                                } else {
                                    Text("Soul Gem: No suitable filled soul gem found! (Uygun dolu ruh tasi bulunamadi!)");
                                }
                            }
                        }
                        Spacing();

                        if (!canAfford) {
                            PushStyleColor(ImGuiCol_Button, ImVec4{0.25f, 0.25f, 0.25f, 1.0f});
                            PushStyleColor(ImGuiCol_Text, ImVec4{0.45f, 0.45f, 0.45f, 1.0f});
                        }

                        bool confirmed = Button("  Confirm (Onayla)  ");

                        if (!canAfford) {
                            PopStyleColor(2);
                            SameLine();
                            TextColored4(1.0f, 0.3f, 0.3f, 1.0f, "Cannot afford! (Karsilanamiyor!)");
                        }

                        if (confirmed && canAfford) {
                            RE::FormID fid = sel.formID;
                            int c = state.confirmCost;
                            auto* tasks = SKSE::GetTaskInterface();
                            if (tasks) {
                                tasks->AddTask([fid, c, requireGold, requireSoulGem, bestGemId, bestGemCount]() {
                                    auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid);
                                    auto& tracker = FollowerTracker::GetSingleton();
                                    const auto reviveCount = tracker.GetReviveCount(fid);
                                    if (!actor || !actor->IsDead() ||
                                        (g_Settings.MaxRevives > 0 && reviveCount >= g_Settings.MaxRevives) ||
                                        (requireGold && GetPlayerGold() < c) ||
                                        (requireSoulGem && CountFilledSoulGems(bestGemId) < bestGemCount)) {
                                        return;
                                    }

                                    if (requireGold) RemovePlayerGold(c);
                                    if (requireSoulGem) RemoveSoulGem(bestGemId, bestGemCount);
                                    if (ReviveFollower(fid)) {
                                        tracker.Remove(fid);
                                    }
                                });
                                state.confirmVisible = false;
                                RefreshList();
                            } else {
                                spdlog::error("LifeAgain: SKSE task interface unavailable; revive cancelled.");
                            }
                        }
                        SameLine();
                        if (Button("  Cancel (Vazgec)  ")) {
                            state.confirmVisible = false;
                        }
                    }
                }
            }
        }
    }

    inline void __stdcall RenderHealingMenu() {
        using namespace ImGuiMCP;
        auto& state = g_MenuState;
        
        static int s_lastFrame = 0;
        int currentFrame = ImGuiMCP::GetFrameCount();
        if (currentFrame - s_lastFrame > 1) {
            RefreshList();
        }
        s_lastFrame = currentFrame;

        // --- YARALANMALAR ---
        TextColored4(1.0f, 0.85f, 0.3f, 1.0f, "--- INJURIES (YARALANMALAR) ---");
        Spacing();

        if (!g_PriestAccess.load()) {
            TextColored4(1.0f, 0.45f, 0.2f, 1.0f,
                "This service is only provided by a priest or monk. (Bu hizmet yalnizca bir rahip veya kesis tarafindan sunulur.)");
            TextColored4(0.65f, 0.65f, 0.65f, 1.0f,
                "Find a religious figure and speak to them. (Bir din adami bulun ve onunla konusun.)");
        } else {
            TextColored4(0.5f, 1.0f, 0.5f, 1.0f,
                "%s is listening to you... (%s sizi dinliyor...)", g_PriestName.c_str(), g_PriestName.c_str());
            
            Spacing();

            if (state.injuredList.empty()) {
                TextColored4(0.7f, 0.7f, 0.7f, 1.0f,
                    "You have no injured followers. (Hic yarali yoldasiniz yok.)");
            } else {
                Text("Total injured followers (Toplam yarali): %d", (int)state.injuredList.size());
                Spacing();

                constexpr int tableFlags =
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

                if (BeginTable("##InjuredTable", 5, tableFlags, ImVec2{0.0f, 200.0f})) {
                    TableSetupColumn("Type (Tur)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    TableSetupColumn("Name (Ad)", ImGuiTableColumnFlags_WidthStretch, 0.0f);
                    TableSetupColumn("Level (Seviye)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    TableSetupColumn("Injury (Durum)", ImGuiTableColumnFlags_WidthFixed, 130.0f);
                    TableSetupColumn("Next heal (Sonraki iyilesme)", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    TableHeadersRow();

                    for (int i = 0; i < (int)state.injuredList.size(); ++i) {
                        const auto& inf = state.injuredList[i];
                        bool selected = (state.selectedInjuredIndex == i);

                        TableNextRow();
                        TableSetColumnIndex(0);
                        Text("%s", KindLabel(inf.kind));

                        TableSetColumnIndex(1);
                        char selId[64];
                        snprintf(selId, sizeof(selId), "%s##inj%d", inf.name.c_str(), i);
                        if (Selectable(selId, selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                            state.selectedInjuredIndex = i;
                            state.confirmHealVisible = false;
                            state.confirmHealWithGold = false;
                            
                            state.confirmHealCost = GetInjuryPotionCost(inf.injuryCounts);
                        }
                        TableSetColumnIndex(2); Text("%d", inf.level);
                        TableSetColumnIndex(3); 
                        if (inf.injuryLevel == InjuryLevel::Heavy) {
                            TextColored4(1.0f, 0.3f, 0.3f, 1.0f, "Heavy (Agir)");
                        } else {
                            TextColored4(1.0f, 0.7f, 0.3f, 1.0f, "Light (Hafif)");
                        }
                        TableSetColumnIndex(4);
                        TextWrapped("%s", NaturalHealingStatus(inf).c_str());
                    }
                    EndTable();
                }

                Spacing();
                if (state.selectedInjuredIndex >= 0 && state.selectedInjuredIndex < (int)state.injuredList.size()) {
                    const auto& sel = state.injuredList[state.selectedInjuredIndex];
                    int cost = state.confirmHealCost;

                    Separator();
                    const int goldCost = GetInjuryGoldCost(sel.injuryCounts);
                    TextColored4(1.0f, 0.9f, 0.5f, 1.0f,
                        "Selected (Secili): %s | Potions (Iksir): %d | Gold (Altin): %d",
                        sel.name.c_str(), cost, goldCost);
                    Text("Wounds (Yaralar):");
                    TextWrapped("%s", DescribeWounds(sel).c_str());
                    TextWrapped("%s", NaturalHealingStatus(sel).c_str());
                    Spacing();

                    if (!state.confirmHealVisible) {
                        if (Button("  Heal with Potions (Iksirle Tedavi)  ")) {
                            state.confirmHealVisible = true;
                            state.confirmHealWithGold = false;
                            state.confirmHealCost = cost;
                        }
                        SameLine();
                        if (Button("  Heal with Gold (Altinla Tedavi)  ")) {
                            state.confirmHealVisible = true;
                            state.confirmHealWithGold = true;
                            state.confirmHealCost = goldCost;
                        }
                    } else {
                        const bool useGold = state.confirmHealWithGold;
                        const int available = useGold ? GetPlayerGold() : GetPlayerCurePotionCount();
                        bool canAfford = (available >= state.confirmHealCost);

                        TextColored4(1.0f, 0.5f, 0.3f, 1.0f,
                            useGold ? "Pay %d Gold to heal %s? (Altinla tedavi edilsin mi?)" : "Use %d Cure Disease Potion(s) to heal %s? (Iksirle tedavi edilsin mi?)",
                            state.confirmHealCost, sel.name.c_str());
                        Text("Available (Mevcut): %d | Gold cost (Altin bedeli): %d",
                            available, goldCost);
                        Spacing();

                        if (!canAfford) {
                            PushStyleColor(ImGuiCol_Button, ImVec4{0.25f, 0.25f, 0.25f, 1.0f});
                            PushStyleColor(ImGuiCol_Text, ImVec4{0.45f, 0.45f, 0.45f, 1.0f});
                        }

                        bool confirmed = Button("  Confirm (Onayla)  ##Heal");

                        if (!canAfford) {
                            PopStyleColor(2);
                            SameLine();
                            TextColored4(1.0f, 0.3f, 0.3f, 1.0f,
                                useGold ? "Not enough gold! (Yeterli altin yok!)" : "Not enough Cure Disease Potions! (Yeterli tedavi iksiri yok!)");
                        }

                        if (confirmed && canAfford) {
                            RE::FormID fid = sel.formID;
                            int c = state.confirmHealCost;
                            const bool payGold = useGold;
                            auto* tasks = SKSE::GetTaskInterface();
                            if (tasks) tasks->AddTask([fid, c, payGold]() {
                                auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid);
                                auto* tracker = &FollowerTracker::GetSingleton();
                                auto inf = tracker->GetInjured(fid);
                                if (!actor || !inf) return;
                                const int currentCost = payGold ? GetInjuryGoldCost(inf->injuryCounts) : GetInjuryPotionCost(inf->injuryCounts);
                                if (currentCost != c) return;
                                if (payGold ? !RemovePlayerGold(c) : !RemovePlayerCurePotions(c)) return;
                                RemoveInjuryDebuffs(actor, &*inf);
                                tracker->RemoveInjured(fid);
                            });
                            else spdlog::error("LifeAgain: SKSE task interface unavailable; healing cancelled.");
                            // Sekme ve secim durumunu hemen sifirla, asenkron temizlik bittikten sonra
                            // bir sonraki frame'de RefreshList otomatik cagrilacak
                            state.selectedInjuredIndex = -1;
                            state.confirmHealVisible = false;
                            RefreshList();
                        }
                        SameLine();
                        if (Button("  Cancel (Vazgec)  ##Heal")) {
                            state.confirmHealVisible = false;
                        }
                    }
                }
            }
        }
    }

    // ============ DASHBOARD ============
    inline void __stdcall RenderDashboard() {
        using namespace ImGuiMCP;

        // Her frame verileri taze tut
        static int s_lastDashFrame = 0;
        int currentFrame = ImGuiMCP::GetFrameCount();
        if (currentFrame - s_lastDashFrame > 60) { // ~1 saniyede bir yenile
            RefreshList();
            s_lastDashFrame = currentFrame;
        }

        auto& state = g_MenuState;

        TextColored4(1.0f, 0.85f, 0.3f, 1.0f, "=== FOLLOWER DASHBOARD (YOLDAS DURUM PANELI) ===");
        Spacing();

        // --- Ozet sayaclar ---
        int totalDead    = (int)state.list.size();
        int totalLight   = 0;
        int totalHeavy   = 0;
        for (auto& inf : state.injuredList) {
            if (inf.injuryLevel == InjuryLevel::Light) totalLight++;
            else if (inf.injuryLevel == InjuryLevel::Heavy) totalHeavy++;
        }

        TextColored4(0.4f, 1.0f, 0.4f, 1.0f, "Healthy (Saglikli): Unknown");
        SameLine();
        TextColored4(1.0f, 0.9f, 0.2f, 1.0f, "  |  Light Injured (Hafif Yarali): %d", totalLight);
        SameLine();
        TextColored4(1.0f, 0.4f, 0.2f, 1.0f, "  |  Heavy Injured (Agir Yarali): %d", totalHeavy);
        SameLine();
        TextColored4(0.6f, 0.6f, 0.6f, 1.0f, "  |  Dead (Oldu): %d", totalDead);
        Spacing();
        Separator();
        Spacing();

        // --- Yarali yoldas tablosu ---
        if (!state.injuredList.empty()) {
            TextColored4(1.0f, 0.85f, 0.3f, 1.0f, "INJURED FOLLOWERS (YARALI YOLDASLAR)");
            Spacing();

            // Tablo
            constexpr int dashboardTableFlags =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
            if (BeginTable("InjuredTable", 7, dashboardTableFlags)) {
                TableSetupColumn("Name (Ad)", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                TableSetupColumn("Type (Tur)", ImGuiTableColumnFlags_WidthFixed, 125.0f);
                TableSetupColumn("Lvl", ImGuiTableColumnFlags_WidthFixed, 48.0f);
                TableSetupColumn("Status (Durum)", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                TableSetupColumn("Wounds (Yaralar)", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                TableSetupColumn("Potions (Iksir)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                TableSetupColumn("Next heal (Sonraki iyilesme)", ImGuiTableColumnFlags_WidthFixed, 205.0f);
                TableHeadersRow();

                for (auto& inf : state.injuredList) {
                    TableNextRow();

                    // Ad
                    TableSetColumnIndex(0);
                    Text("%s", inf.name.c_str());

                    // Tur
                    TableSetColumnIndex(1);
                    Text("%s", KindLabel(inf.kind));

                    // Seviye
                    TableSetColumnIndex(2);
                    Text("%d", inf.level);

                    // Durum (renkli)
                    TableSetColumnIndex(3);
                    if (inf.injuryLevel == InjuryLevel::Light) {
                        TextColored4(1.0f, 0.9f, 0.2f, 1.0f, "Light (Hafif)");
                    } else {
                        TextColored4(1.0f, 0.35f, 0.2f, 1.0f, "Heavy (Agir)");
                    }

                    TableSetColumnIndex(4);
                    TextWrapped("%s", DescribeWounds(inf).c_str());

                    TableSetColumnIndex(5);
                    Text("%d", GetInjuryPotionCost(inf.injuryCounts));

                    TableSetColumnIndex(6);
                    TextWrapped("%s", NaturalHealingStatus(inf).c_str());
                }
                EndTable();
            }
            Spacing();
        } else {
            TextColored4(0.4f, 1.0f, 0.4f, 1.0f, "No injured followers! (Yarali yoldas yok!)");
            Spacing();
        }

        Separator();
        Spacing();

        // --- Olmus yoldas tablosu ---
        if (!state.list.empty()) {
            TextColored4(1.0f, 0.85f, 0.3f, 1.0f, "DEAD FOLLOWERS (OLMUS YOLDASLAR)");
            Spacing();

            if (BeginTable("DeadTable", 4, 0)) {
                // Manuel baslik satiri
                TableNextRow();
                TableSetColumnIndex(0); Text("Name (Ad)");
                TableSetColumnIndex(1); Text("Type (Tur)");
                TableSetColumnIndex(2); Text("Lvl");
                TableSetColumnIndex(3); Text("Death Date (Olum Tarihi)");
                Separator();

                for (auto& df : state.list) {
                    TableNextRow();

                    TableSetColumnIndex(0);
                    TextColored4(0.7f, 0.7f, 0.7f, 1.0f, "%s", df.name.c_str());

                    TableSetColumnIndex(1);
                    Text("%s", KindLabel(df.kind));

                    TableSetColumnIndex(2);
                    Text("%d", df.level);

                    TableSetColumnIndex(3);
                    Text("%s", df.deathDate.c_str());
                }
                EndTable();
            }
            Spacing();
        } else {
            TextColored4(0.4f, 1.0f, 0.4f, 1.0f, "No dead followers! (Olmus yoldas yok!)");
        }

        Spacing();
        if (Button("  Refresh (Yenile)  ")) {
            RefreshList();
        }
    }

    inline void InitializeMenu() {
        if (!SKSEMenuFramework::IsInstalled()) {
            spdlog::warn("LifeAgain: SKSEMenuFramework.dll bulunamadi! Menu devre disi.");
            return;
        }

        SKSEMenuFramework::SetSection("Life Again");
        SKSEMenuFramework::AddSectionItem("Settings (Ayarlar)", RenderSettingsMenu);
        SKSEMenuFramework::AddSectionItem("Revival (Diriltme)", RenderRevivalMenu);
        SKSEMenuFramework::AddSectionItem("Healing (Iyilesme)", RenderHealingMenu);
        SKSEMenuFramework::AddSectionItem("Dashboard (Pano)", RenderDashboard);
        SKSEMenuFramework::AddSectionItem("Injury Test Lab (Yaralanma Testi)", RenderInjuryTestMenu);

        spdlog::info("LifeAgain: SKSE Menu - Settings + Revival + Healing + Dashboard sekmeleri kayit edildi.");
    }

}  // namespace LifeAgain
