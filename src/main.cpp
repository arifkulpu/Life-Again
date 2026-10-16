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
#include "KingCore/KingCoreAPI.h"

using namespace LifeAgain;
using namespace std::literals;

// ---- SKSE Plugin Bildirimi ----
SKSEPluginInfo(
    .Version              = REL::Version{1, 2, 1, 0},
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
            constexpr std::uint32_t kMaxRecordSize = 10 * 1024 * 1024;
            if (length < sizeof(std::uint32_t) || length > kMaxRecordSize + sizeof(std::uint32_t)) {
                spdlog::warn("LifeAgain: Gecersiz kayit boyutu: {}", length);
                continue;
            }

            std::uint32_t len = 0;
            if (a_intfc->ReadRecordData(&len, sizeof(len)) != sizeof(len)) {
                spdlog::warn("LifeAgain: Kayit uzunlugu okunamadi.");
                continue;
            }
            if (len == 0 || len > kMaxRecordSize || len != length - sizeof(len)) {
                spdlog::warn("LifeAgain: Gecersiz veri uzunlugu: {}", len);
                continue;
            }
            std::string data(len, '\0');
            if (a_intfc->ReadRecordData(data.data(), len) != len) {
                spdlog::warn("LifeAgain: Kayit verisi eksik okundu.");
                continue;
            }
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

        const std::string treatmentLogPath = (logDir / "LifeAgain_Treatment.log").string();
        auto treatmentSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(treatmentLogPath, true);
        auto treatmentLog = std::make_shared<spdlog::logger>("LifeAgainTreatment", treatmentSink);
        treatmentLog->set_level(spdlog::level::debug);
        treatmentLog->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        treatmentLog->flush_on(spdlog::level::debug);
        spdlog::register_logger(treatmentLog);
        treatmentLog->info("Treatment diagnostics started. Log: {}", treatmentLogPath);
    } catch (const std::exception& e) {
        (void)e;
    }
}

// ---- King Core uyumlulugu: tedavi esyalarini King Core'a bildir ----
// King Core bu kaydi tuketici modlara (Follower Unbound) relay eder; boylece
// takipci otomasyonu (satis, envanter temizligi) tedavi iksirlerini Life Again
// iyilestirme sirasinda satilmasini veya atilmasini engeller.
void AnnounceTreatmentItems() {
    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging) return;

    KingCore::TreatmentRegistration reg{};
    reg.apiVersion = KingCore::API_VERSION;

    if (auto* potion = RE::TESForm::LookupByID(0x000AE723); potion && potion->GetFormType() == RE::FormType::AlchemyItem) {
        reg.items[reg.itemCount].formID = potion->GetFormID();
        reg.itemCount++;
        spdlog::info("LifeAgain: Cure Disease Potion duyuruluyor (FormID: {:08X})", potion->GetFormID());
    }

    if (reg.itemCount == 0) {
        spdlog::info("LifeAgain: Cure Disease Potion formu bulunamadi - tedavi esyasi duyurusu atlandi.");
        return;
    }

    // King Core'a yonelik + yayin (King Core kurulu degilse tuketiciler yine de alsin)
    messaging->Dispatch(KingCore::MESSAGE_TYPE_REGISTER_TREATMENT, &reg, sizeof(reg), "KingCore");
    messaging->Dispatch(KingCore::MESSAGE_TYPE_REGISTER_TREATMENT, &reg, sizeof(reg), nullptr);
    spdlog::info("LifeAgain: King Core'a {} tedavi esyasi kaydedildi.", reg.itemCount);
}

// ---- SKSE Mesaj Dinleyicisi ----
void OnSKSEMessage(SKSE::MessagingInterface::Message* msg) {
    if (!msg) return;

    switch (msg->type) {
        case KingCore::MESSAGE_TYPE_FOLLOWER_RESTED:
            {
                if (!msg->sender || _stricmp(msg->sender, "KingCore") != 0 ||
                    !msg->data || msg->dataLen < sizeof(KingCore::FollowerRestEvent)) break;
                const auto* event = static_cast<const KingCore::FollowerRestEvent*>(msg->data);
                const auto providerMajor = (event->apiVersion >> 16) & 0xFFFF;
                if (providerMajor != KingCore::API_VERSION_MAJOR ||
                    (event->type != KingCore::RestEventType::Wait && event->type != KingCore::RestEventType::Sleep)) break;

                const auto eventDay = event->gameDaysPassed;
                if (auto* taskInterface = SKSE::GetTaskInterface()) {
                    taskInterface->AddTask([eventDay]() { ProcessNaturalLightWoundHealing(eventDay); });
                }
            }
            break;
        case LifeAgain::API::kRequestFollowerInjurySnapshotMessage:
            if (msg->sender && _stricmp(msg->sender, "FollowerUnbound") == 0) {
                if (auto* taskInterface = SKSE::GetTaskInterface()) {
                    taskInterface->AddTask([]() { PublishFollowerInjurySnapshot(); });
                }
            }
            break;
        case SKSE::MessagingInterface::kPostLoad:
            spdlog::info("LifeAgain: kPostLoad - Menu baslatiliyor...");
            InitializeMenu();
            spdlog::info("LifeAgain: Menu baslatildi.");
            break;

        case SKSE::MessagingInterface::kDataLoaded:
            spdlog::info("LifeAgain: kDataLoaded - Event sink'ler kayit ediliyor...");
            RegisterDeathEventSink();
            AnnounceTreatmentItems();
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

        case SKSE::MessagingInterface::kPostLoadGame:
            // Save yuklendikten sonra: yara overlay/OAR durumlarini yenile
            spdlog::info("LifeAgain: kPostLoadGame - Yaralanma durumlari yenileniyor...");
            if (auto* taskInterface = SKSE::GetTaskInterface()) {
                taskInterface->AddTask([]() {
                    PublishFollowerInjurySnapshot();
                    RefreshInjuryBridgeOnLoad();
                });
            }
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
    if (!messaging->RegisterListener("FollowerUnbound", OnSKSEMessage)) {
        spdlog::warn("LifeAgain: FollowerUnbound message listener registration failed.");
    }
    if (!messaging->RegisterListener("KingCore", OnSKSEMessage)) {
        spdlog::warn("LifeAgain: King Core message listener registration failed.");
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
