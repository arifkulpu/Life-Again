#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <shlobj_core.h>
#include <filesystem>
#include <string>
#include <system_error>

#include "Settings.h"
#include "FollowerTracker.h"
#include "MenuManager.h"

using namespace LifeAgain;
using namespace std::literals;

// ---- SKSE Plugin Bildirimi ----
SKSEPluginInfo(
    .Version              = REL::Version{1, 2, 0, 0},
    .Name                 = "LifeAgain"sv,
    .Author               = "Arif KULPU"sv,
    .SupportEmail         = "support@example.com"sv,
    .StructCompatibility  = SKSE::StructCompatibility::Independent,
    .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary,
    .MinimumSKSEVersion   = REL::Version{2, 0, 0, 0}
);

// Serialization kayit turu: 'AFIL' (4 byte)
static constexpr std::uint32_t kRecordType    = 'AFIL';
static constexpr std::uint32_t kRecordVersion = 1;

// ---- Kaydet (Save) ----
void OnSKSESave(SKSE::SerializationInterface* a_intfc) {
    spdlog::info("LifeAgain: Save callback tetiklendi.");
    std::string data = FollowerTracker::GetSingleton().SerializeToJsonString();
    if (data.empty()) return;

    if (a_intfc->OpenRecord(kRecordType, kRecordVersion)) {
        std::uint32_t len = static_cast<std::uint32_t>(data.size());
        a_intfc->WriteRecordData(&len, sizeof(len));
        a_intfc->WriteRecordData(data.data(), len);
        spdlog::info("LifeAgain: {} byte veri save dosyasina yazildi.", len);
    } else {
        spdlog::error("LifeAgain: OpenRecord basarisiz!");
    }
}

// ---- Yukle (Load) ----
void OnSKSELoad(SKSE::SerializationInterface* a_intfc) {
    spdlog::info("LifeAgain: Load callback tetiklendi.");
    FollowerTracker::GetSingleton().Clear();

    std::uint32_t type = 0, version = 0, length = 0;
    while (a_intfc->GetNextRecordInfo(type, version, length)) {
        if (type == kRecordType) {
            std::uint32_t len = 0;
            a_intfc->ReadRecordData(&len, sizeof(len));
            if (len == 0 || len > 10 * 1024 * 1024) {
                spdlog::warn("LifeAgain: Gecersiz veri uzunlugu: {}", len);
                continue;
            }
            std::string data(len, '\0');
            a_intfc->ReadRecordData(data.data(), len);
            FollowerTracker::GetSingleton().DeserializeFromJsonString(data, a_intfc);
            spdlog::info("LifeAgain: {} byte veri save dosyasindan yuklendi.", len);
        }
    }
}

// ---- Geri Al / Revert ----
void OnSKSERevert(SKSE::SerializationInterface* /*a_intfc*/) {
    spdlog::info("LifeAgain: Revert callback - veriler temizleniyor...");
    FollowerTracker::GetSingleton().Clear();
}

void InitializeLogging() {
    // SKSE standart log dizini: Documents\My Games\Skyrim Special Edition\SKSE
    std::filesystem::path logDir;
    PWSTR docPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &docPath))) {
        logDir = std::filesystem::path(docPath) / "My Games" / "Skyrim Special Edition" / "SKSE";
        CoTaskMemFree(docPath);
    } else {
        logDir = std::filesystem::current_path();
    }

    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);
    const std::string logPath = (logDir / "LifeAgain.log").string();

    try {
        auto sink = std::shared_ptr<spdlog::sinks::basic_file_sink_mt>(
            new spdlog::sinks::basic_file_sink_mt(logPath, true));
        auto log = std::make_shared<spdlog::logger>("LifeAgain", sink);
        log->set_level(spdlog::level::debug);
        log->flush_on(spdlog::level::debug);
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S] [%l] %v");
        spdlog::info("LifeAgain: Log baslatildi. Yol: {}", logPath);
    } catch (const std::exception& e) {
        (void)e;
    }
}

// ---- SKSE Mesaj Dinleyicisi ----
void OnSKSEMessage(SKSE::MessagingInterface::Message* msg) {
    if (!msg) return;

    switch (msg->type) {
        case SKSE::MessagingInterface::kPostLoad:
            spdlog::info("LifeAgain: kPostLoad - Menu baslatiliyor...");
            InitializeMenu();
            spdlog::info("LifeAgain: Menu baslatildi.");
            break;

        case SKSE::MessagingInterface::kDataLoaded:
            spdlog::info("LifeAgain: kDataLoaded - Event sink'ler kayit ediliyor...");
            RegisterDeathEventSink();
            break;

        case SKSE::MessagingInterface::kNewGame:
            // Yeni oyun baslatildi - verileri tamamen sifirla
            spdlog::info("LifeAgain: kNewGame - Takipci verileri sifirlanıyor...");
            FollowerTracker::GetSingleton().Clear();
            break;

        case SKSE::MessagingInterface::kPreLoadGame:
            // Kayit dosyasi yuklenmeden once bellegi temizle
            // (SKSE Revert+Load callback da bunu yapiyor, bu ek güvence)
            spdlog::info("LifeAgain: kPreLoadGame - Eski veriler temizleniyor...");
            FollowerTracker::GetSingleton().Clear();
            break;

        default:
            break;
    }
}

// ---- Ana Giris Noktasi ----
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    InitializeLogging();
    spdlog::info("LifeAgain v1.2 yuklendi.");

    // Ayarlari dosyadan yukle
    LoadSettings();
    spdlog::info("LifeAgain: GoldMultiplier = {}", g_Settings.GoldMultiplier);

    // SKSE mesaj sistemi
    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnSKSEMessage)) {
        spdlog::error("LifeAgain: MessagingInterface kaydi basarisiz!");
        return false;
    }

    // SKSE serillestirme sistemi - her save dosyasina ayri veri
    auto* serialization = SKSE::GetSerializationInterface();
    if (!serialization) {
        spdlog::error("LifeAgain: SerializationInterface alinamadi!");
        return false;
    }
    serialization->SetUniqueID(kRecordType);
    serialization->SetSaveCallback(OnSKSESave);
    serialization->SetLoadCallback(OnSKSELoad);
    serialization->SetRevertCallback(OnSKSERevert);
    spdlog::info("LifeAgain: SerializationInterface kaydedildi.");

    spdlog::info("LifeAgain: Yukleme tamamlandi.");
    return true;
}
