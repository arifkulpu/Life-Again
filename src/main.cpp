#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <shlobj_core.h>
#include <filesystem>

#include "Settings.h"
#include "FollowerTracker.h"
#include "MenuManager.h"

using namespace LifeAgain;
using namespace std::literals;

// ---- SKSE Plugin Bildirimi ----
SKSEPluginInfo(
    .Version              = REL::Version{1, 0, 0, 0},
    .Name                 = "LifeAgain"sv,
    .Author               = "Your Name"sv,
    .SupportEmail         = "support@example.com"sv,
    .StructCompatibility  = SKSE::StructCompatibility::Independent,
    .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary,
    .MinimumSKSEVersion   = REL::Version{2, 0, 0, 0}
);

void InitializeLogging() {
    std::filesystem::path logPath = "C:\\Users\\pc\\Desktop\\LifeAgain.log";

    try {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
        auto log  = std::make_shared<spdlog::logger>("LifeAgain", std::move(sink));
        log->set_level(spdlog::level::debug);
        log->flush_on(spdlog::level::debug);
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S] [%l] %v");
        spdlog::info("LifeAgain: Log baslatildi. Yol: {}", logPath.string());
    } catch (const std::exception& e) {
        // Log başlatılamadı - sessizce devam et
        (void)e;
    }
}

// ---- SKSE Mesaj Dinleyicisi ----
void OnSKSEMessage(SKSE::MessagingInterface::Message* msg) {
    if (!msg) return;

    switch (msg->type) {
        case SKSE::MessagingInterface::kPostLoad:
            // Tüm DLL'ler yüklendi → Framework penceresi oluştur
            spdlog::info("LifeAgain: kPostLoad - Menu baslatiliyor...");
            InitializeMenu();
            spdlog::info("LifeAgain: Menu baslatildi.");
            break;

        case SKSE::MessagingInterface::kDataLoaded:
            // Oyun verileri yüklendi → Event sink'leri bağla
            spdlog::info("LifeAgain: kDataLoaded - Event sink'ler kayit ediliyor...");
            RegisterDeathEventSink();
            FollowerTracker::GetSingleton().LoadFromFile();
            spdlog::info("LifeAgain: {} kayitli oldu yoldas yuklendi.",
                FollowerTracker::GetSingleton().GetDeadFollowers().size());
            break;

        default:
            break;
    }
}

// ---- Ana Giriş Noktası ----
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    InitializeLogging();
    spdlog::info("LifeAgain v1.0 yuklendi.");

    // Ayarları dosyadan yükle
    LoadSettings();
    spdlog::info("LifeAgain: GoldMultiplier = {}", g_Settings.GoldMultiplier);
    spdlog::info("LifeAgain: BleedoutDeathChance_Healthy = {}", g_Settings.BleedoutDeathChance_Healthy);

    // SKSE mesaj sistemi
    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnSKSEMessage)) {
        spdlog::error("LifeAgain: MessagingInterface kaydi basarisiz!");
        return false;
    }

    spdlog::info("LifeAgain: MessagingInterface kaydedildi, yukleme tamamlandi.");
    return true;
}
