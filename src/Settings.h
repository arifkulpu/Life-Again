#pragma once
#include <SKSE/SKSE.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace LifeAgain {

    struct Settings {
        // Diriltme bedeli = yoldas_seviyesi * GoldMultiplier
        int GoldMultiplier = 500;
        int BleedoutDeathChance_Healthy = 5;
        int BleedoutDeathChance_Light = 15;
        int BleedoutDeathChance_Heavy = 40;
        
        // Tedavi bedeli = yoldas_seviyesi * InjuryCostMultiplier
        int LightInjuryCostMultiplier = 100;
        int HeavyInjuryCostMultiplier = 300;

        bool ShowDeathNotification = true;
        bool ShowDeathMessageBox = false;

        int InjuryChance = 15;
        bool ShowInjuryNotification = true;
        bool ShowInjuryMessageBox = false;

        int MaxRevives = 3;

        int PotionRequirementLight = 3;
        int PotionRequirementHeavy = 10;
        
        bool UseGoldForRevive = true;
        bool UseSoulGemsForRevive = true;
    };

    inline Settings g_Settings;

    inline std::filesystem::path GetSettingsPath() {
        auto dir = SKSE::log::log_directory();
        if (!dir) return {};
        return dir->parent_path() / "LifeAgain" / "settings.json";
    }

    inline void LoadSettings() {
        auto path = GetSettingsPath();
        if (!std::filesystem::exists(path)) return;
        try {
            std::ifstream f(path);
            auto j = nlohmann::json::parse(f);
            if (j.contains("GoldMultiplier")) g_Settings.GoldMultiplier = j["GoldMultiplier"].get<int>();
            if (j.contains("BleedoutDeathChance_Healthy")) g_Settings.BleedoutDeathChance_Healthy = j["BleedoutDeathChance_Healthy"].get<int>();
            if (j.contains("BleedoutDeathChance_Light")) g_Settings.BleedoutDeathChance_Light = j["BleedoutDeathChance_Light"].get<int>();
            if (j.contains("BleedoutDeathChance_Heavy")) g_Settings.BleedoutDeathChance_Heavy = j["BleedoutDeathChance_Heavy"].get<int>();
            if (j.contains("LightInjuryCostMultiplier")) g_Settings.LightInjuryCostMultiplier = j["LightInjuryCostMultiplier"].get<int>();
            if (j.contains("HeavyInjuryCostMultiplier")) g_Settings.HeavyInjuryCostMultiplier = j["HeavyInjuryCostMultiplier"].get<int>();
            if (j.contains("ShowDeathNotification")) g_Settings.ShowDeathNotification = j["ShowDeathNotification"].get<bool>();
            if (j.contains("ShowDeathMessageBox")) g_Settings.ShowDeathMessageBox = j["ShowDeathMessageBox"].get<bool>();
            if (j.contains("InjuryChance")) g_Settings.InjuryChance = j["InjuryChance"].get<int>();
            if (j.contains("ShowInjuryNotification")) g_Settings.ShowInjuryNotification = j["ShowInjuryNotification"].get<bool>();
            if (j.contains("ShowInjuryMessageBox")) g_Settings.ShowInjuryMessageBox = j["ShowInjuryMessageBox"].get<bool>();
            if (j.contains("MaxRevives")) g_Settings.MaxRevives = j["MaxRevives"].get<int>();
            if (j.contains("PotionRequirementLight")) g_Settings.PotionRequirementLight = j["PotionRequirementLight"].get<int>();
            if (j.contains("PotionRequirementHeavy")) g_Settings.PotionRequirementHeavy = j["PotionRequirementHeavy"].get<int>();
            if (j.contains("UseGoldForRevive")) g_Settings.UseGoldForRevive = j["UseGoldForRevive"].get<bool>();
            if (j.contains("UseSoulGemsForRevive")) g_Settings.UseSoulGemsForRevive = j["UseSoulGemsForRevive"].get<bool>();
        } catch (...) {}
    }

    inline void SaveSettings() {
        auto path = GetSettingsPath();
        std::filesystem::create_directories(path.parent_path());
        try {
            nlohmann::json j;
            j["GoldMultiplier"] = g_Settings.GoldMultiplier;
            j["BleedoutDeathChance_Healthy"] = g_Settings.BleedoutDeathChance_Healthy;
            j["BleedoutDeathChance_Light"] = g_Settings.BleedoutDeathChance_Light;
            j["BleedoutDeathChance_Heavy"] = g_Settings.BleedoutDeathChance_Heavy;
            j["LightInjuryCostMultiplier"] = g_Settings.LightInjuryCostMultiplier;
            j["HeavyInjuryCostMultiplier"] = g_Settings.HeavyInjuryCostMultiplier;
            j["ShowDeathNotification"] = g_Settings.ShowDeathNotification;
            j["ShowDeathMessageBox"] = g_Settings.ShowDeathMessageBox;
            j["InjuryChance"] = g_Settings.InjuryChance;
            j["ShowInjuryNotification"] = g_Settings.ShowInjuryNotification;
            j["ShowInjuryMessageBox"] = g_Settings.ShowInjuryMessageBox;
            j["MaxRevives"] = g_Settings.MaxRevives;
            j["PotionRequirementLight"] = g_Settings.PotionRequirementLight;
            j["PotionRequirementHeavy"] = g_Settings.PotionRequirementHeavy;
            j["UseGoldForRevive"] = g_Settings.UseGoldForRevive;
            j["UseSoulGemsForRevive"] = g_Settings.UseSoulGemsForRevive;
            
            std::ofstream f(path);
            f << j.dump(4);
        } catch (...) {}
    }
}
