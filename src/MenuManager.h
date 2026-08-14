#pragma once
#include <SKSE/SKSE.h>
#include <RE/I/InputEvent.h>
#include <RE/B/ButtonEvent.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESBoundObject.h>
#include <RE/B/BSInputDeviceManager.h>
#include <SKSEMenuFramework.h>
#include <spdlog/spdlog.h>
#include "FollowerTracker.h"
#include "Settings.h"
#include <vector>
#include <string>
#include <mutex>

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

    inline bool GetAvailableSoulGem(RE::FormID& outFormID, int& outCount, std::string& outName) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;

        auto inv = player->GetInventory();
        for (const auto& gem : GetSoulGemHierarchy()) {
            for (auto& [form, data] : inv) {
                if (form && form->GetFormID() == gem.id) {
                    if (data.first >= gem.requiredAmount) {
                        outFormID = gem.id;
                        outCount = gem.requiredAmount;
                        outName = gem.name;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    inline void RemoveSoulGem(RE::FormID formID, int count) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;
        auto* form = RE::TESForm::LookupByID(formID);
        if (!form) return;
        auto* boundObj = static_cast<RE::TESBoundObject*>(form);
        if (!boundObj) return;
        player->RemoveItem(boundObj, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
    }

    inline SKSEMenuFramework::Model::WindowInterface* g_Window = nullptr;

    inline int GetPlayerGold() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return 0;
        auto* goldForm = static_cast<RE::TESBoundObject*>(RE::TESForm::LookupByID(0x0000000F));
        if (!goldForm) return 0;
        return player->GetItemCount(goldForm);
    }

    inline void RemovePlayerGold(int amount) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;
        auto* goldForm = static_cast<RE::TESBoundObject*>(RE::TESForm::LookupByID(0x0000000F));
        if (!goldForm) return;
        player->RemoveItem(goldForm, amount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
    }

    inline void ReviveFollower(RE::FormID formID) {
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
        if (!actor) return;
        if (actor->IsDead()) {
            actor->Resurrect(false, false);
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            actor->MoveTo(player->AsReference());
        }
        
        FollowerTracker::GetSingleton().AddReviveCount(formID);
        spdlog::info("LifeAgain: {} hayata donduruldu", actor->GetDisplayFullName());
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
        int  confirmCost     = 0;
        int  confirmHealCost = 0;
    };

    inline MenuState  g_MenuState;
    inline std::mutex g_MenuMutex;

    inline void RefreshList() {
        std::lock_guard lock(g_MenuMutex);
        CheckNaturalHealing(); // Zamanli dogal iyilesmeleri kontrol et
        
        auto newList = FollowerTracker::GetSingleton().GetDeadFollowers();
        auto newInjuredList = FollowerTracker::GetSingleton().GetInjuredFollowers();
        
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

        if (SliderInt("Light Injury Cost Multiplier (Hafif Yarali Tedavi Carpani)", &g_Settings.LightInjuryCostMultiplier, 10, 1000)) {
            SaveSettings();
        }
        if (SliderInt("Heavy Injury Cost Multiplier (Agir Yarali Tedavi Carpani)", &g_Settings.HeavyInjuryCostMultiplier, 10, 1000)) {
            SaveSettings();
        }
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
        
        if (SliderInt("Injury Chance % (Yaralanma Ihtimali)", &g_Settings.InjuryChance, 0, 100)) {
            SaveSettings();
        }
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
        
        if (SliderInt("Light Injury Potion Cost (Hafif Yarali Iksir Maliyeti)", &g_Settings.PotionRequirementLight, 1, 20)) {
            SaveSettings();
        }
        if (SliderInt("Heavy Injury Potion Cost (Agir Yarali Iksir Maliyeti)", &g_Settings.PotionRequirementHeavy, 1, 30)) {
            SaveSettings();
        }
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
                "This service is only provided by a priest in a temple. (Bu hizmet yalnizca tapinaktaki bir rahip tarafindan sunulur.)");
            TextColored4(0.65f, 0.65f, 0.65f, 1.0f,
                "Go to a temple and speak to a priest. (Bir tapinaga gidin ve bir rahiple konusun.)");
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
                            SKSE::GetTaskInterface()->AddTask([fid, c, requireGold, requireSoulGem, bestGemId, bestGemCount]() {
                                if (requireGold) RemovePlayerGold(c);
                                if (requireSoulGem) RemoveSoulGem(bestGemId, bestGemCount);
                                ReviveFollower(fid);
                            });
                            FollowerTracker::GetSingleton().Remove(fid);
                            RefreshList();
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
                "This service is only provided by a priest in a temple. (Bu hizmet yalnizca tapinaktaki bir rahip tarafindan sunulur.)");
            TextColored4(0.65f, 0.65f, 0.65f, 1.0f,
                "Go to a temple and speak to a priest. (Bir tapinaga gidin ve bir rahiple konusun.)");
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

                if (BeginTable("##InjuredTable", 4, tableFlags, ImVec2{0.0f, 200.0f})) {
                    TableSetupColumn("Type (Tur)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    TableSetupColumn("Name (Ad)", ImGuiTableColumnFlags_WidthStretch, 0.0f);
                    TableSetupColumn("Level (Seviye)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    TableSetupColumn("Injury (Durum)", ImGuiTableColumnFlags_WidthFixed, 130.0f);
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
                            
                            int mult = (inf.injuryLevel == InjuryLevel::Heavy) ? g_Settings.HeavyInjuryCostMultiplier : g_Settings.LightInjuryCostMultiplier;
                            state.confirmHealCost = inf.level * mult;
                        }
                        TableSetColumnIndex(2); Text("%d", inf.level);
                        TableSetColumnIndex(3); 
                        if (inf.injuryLevel == InjuryLevel::Heavy) {
                            TextColored4(1.0f, 0.3f, 0.3f, 1.0f, "Heavy (Agir)");
                        } else {
                            TextColored4(1.0f, 0.7f, 0.3f, 1.0f, "Light (Hafif)");
                        }
                    }
                    EndTable();
                }

                Spacing();
                if (state.selectedInjuredIndex >= 0 && state.selectedInjuredIndex < (int)state.injuredList.size()) {
                    const auto& sel = state.injuredList[state.selectedInjuredIndex];
                    int cost = state.confirmHealCost;

                    Separator();
                    TextColored4(1.0f, 0.9f, 0.5f, 1.0f,
                        "Selected (Secili): %s  |  Level (Seviye): %d  |  Cost (Maliyet): %d Gold",
                        sel.name.c_str(), sel.level, cost);
                    Spacing();

                    if (!state.confirmHealVisible) {
                        if (Button("  Heal (Tedavi Et)  ")) {
                            state.confirmHealVisible = true;
                            state.confirmHealCost = cost;
                        }
                    } else {
                        int playerGold = GetPlayerGold();
                        bool canAfford = (playerGold >= state.confirmHealCost);

                        TextColored4(1.0f, 0.5f, 0.3f, 1.0f, "It will cost %d Gold for %s. Are you sure? (Emin misiniz?)", state.confirmHealCost, sel.name.c_str());
                        Text("Current Gold (Mevcut Altin): %d", playerGold);
                        Spacing();

                        if (!canAfford) {
                            PushStyleColor(ImGuiCol_Button, ImVec4{0.25f, 0.25f, 0.25f, 1.0f});
                            PushStyleColor(ImGuiCol_Text, ImVec4{0.45f, 0.45f, 0.45f, 1.0f});
                        }

                        bool confirmed = Button("  Confirm (Onayla)  ##Heal");

                        if (!canAfford) {
                            PopStyleColor(2);
                            SameLine();
                            TextColored4(1.0f, 0.3f, 0.3f, 1.0f, "Not enough gold! (Yeterli altin yok!)");
                        }

                        if (confirmed && canAfford) {
                            RE::FormID fid = sel.formID;
                            int c = state.confirmHealCost;
                            SKSE::GetTaskInterface()->AddTask([fid, c]() {
                                RemovePlayerGold(c);
                                auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid);
                                if (actor) {
                                    auto* tracker = &FollowerTracker::GetSingleton();
                                    InjuredFollower* inf = tracker->GetInjured(fid);
                                    if (inf) {
                                        RemoveInjuryDebuffs(actor, inf->injuryLevel);
                                    }
                                    tracker->RemoveInjured(fid);
                                }
                            });
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
        auto* cal = RE::Calendar::GetSingleton();
        float today = cal ? cal->GetDaysPassed() : 0.0f;

        TextColored4(1.0f, 0.85f, 0.3f, 1.0f, "=== FOLLOWER DASHBOARD (YOLDAS DURUM PANELI) ===");
        Spacing();

        // --- Ozet sayaclar ---
        int totalDead    = (int)state.list.size();
        int totalLight   = 0;
        int totalHeavy   = 0;
        for (auto& inf : state.injuredList) {
            if (inf.injuryLevel == InjuryLevel::Light) totalLight++;
            else                                        totalHeavy++;
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
            if (BeginTable("InjuredTable", 6, 0)) {
                // Manuel baslik satiri
                TableNextRow();
                TableSetColumnIndex(0); Text("Name (Ad)");
                TableSetColumnIndex(1); Text("Type (Tur)");
                TableSetColumnIndex(2); Text("Lvl");
                TableSetColumnIndex(3); Text("Status (Durum)");
                TableSetColumnIndex(4); Text("Bleedouts");
                TableSetColumnIndex(5); Text("Heals In (Iyilesme)");
                Separator();

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

                    // Bleedout sayisi
                    TableSetColumnIndex(4);
                    Text("%d", inf.bleedoutCount);

                    // Dogal iyilesme suresi
                    TableSetColumnIndex(5);
                    if (inf.injuryLevel == InjuryLevel::Heavy) {
                        TextColored4(0.7f, 0.4f, 0.4f, 1.0f, "Temple only");
                    } else {
                        float daysLeft = 3.0f - (today - inf.lastBleedoutDay);
                        if (daysLeft <= 0.0f) {
                            TextColored4(0.4f, 1.0f, 0.4f, 1.0f, "Recovering...");
                        } else {
                            Text("%.1f day(s) (gun)", daysLeft);
                        }
                    }
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

        spdlog::info("LifeAgain: SKSE Menu - Settings + Revival + Healing + Dashboard sekmeleri kayit edildi.");
    }

}  // namespace LifeAgain
