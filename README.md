# Life Again — SKSE Plugin

> **A Skyrim Special Edition / Anniversary Edition SKSE plugin that completely overhauls the follower death and injury system.**  
> **Skyrim'deki yoldaş ölüm ve yaralanma sistemini kökten yenileyen bir SKSE eklentisi.**

---

## 📋 Requirements / Gereksinimler

| Requirement | Version |
|---|---|
| Skyrim Special / Anniversary Edition | Latest |
| SKSE64 | 2.2.x+ |
| Address Library for SKSE | Latest |
| SKSE Menu Framework | Latest |

---

## 📦 Installation / Kurulum

1. Download the latest `LifeAgain.dll`.
2. Place it in your Skyrim installation folder:  
   `Data\SKSE\Plugins\LifeAgain.dll`
3. Launch Skyrim through SKSE.

---

1. En son `LifeAgain.dll` dosyasını indirin.
2. Skyrim kurulum klasörünüzün içine koyun:  
   `Data\SKSE\Plugins\LifeAgain.dll`
3. Skyrim'i SKSE üzerinden başlatın.

---

## 🚀 Changelog / Güncellemeler

### Version 1.1
**EN:**
- **Crash Fix:** Fixed critical random crashes on follower death by delegating events to the SKSE task interface instead of executing directly inside the event sink.
- **Soul Gem Revival System:** You can now revive followers using filled Soul Gems instead of (or alongside) gold. Black, Grand, Greater, Common, Lesser, and Petty soul gems are supported with scaling costs based on their power.
- **Wandering Priests & New Religions:** Revival and healing services are no longer strictly limited to Temples. You can now talk to wandering priests, Vigilants of Stendarr, monks, and other religious figures out in the wild to perform these services.
- **Configurable Potion Requirements:** The amount of Cure Disease Potions required to heal followers in the field is now configurable via the MCM for both light and heavy injuries.
- **MCM Toggles:** New toggles added for Gold vs. Soul Gem revive costs, as well as distinct sections for reviving and healing.

**TR:**
- **Çökme Düzeltmesi:** Yoldaş ölümlerinde aniden yaşanan çökmeler (crash) düzeltildi.
- **Ruh Taşı ile Diriltme:** Artık yoldaşlarınızı sadece altınla değil, dolu Ruh Taşları ile de diriltebilirsiniz. Siyah, Yüce, Büyük, Yaygın, Orta ve Küçük ruh taşları desteklenmektedir (MCM üzerinden açılıp kapatılabilir).
- **Gezgin Rahipler ve Yeni Dinler:** Diriltme ve iyileştirme işlemleri artık sadece tapınaklarla sınırlı değil. Yollarda gezen Stendarr Muhafızları (Vigilants of Stendarr), keşişler ve diğer din adamları da bu işlemleri yapabilecek.
- **Ayarlanabilir İksir İhtiyacı:** Yoldaşları sahada iyileştirmek için gereken Tedavi İksiri sayısı hafif ve ağır yaralanmalar için artık MCM üzerinden ayrı ayrı ayarlanabiliyor.
- **MCM Sekmeleri:** Menüler daha düzenli hale getirildi ve Ruh Taşı/Altın seçenekleri eklendi.

---

## ✨ Features / Özellikler

---

### 🏴 Follower Death System / Yoldaş Ölüm Sistemi

**EN:**  
When a follower goes down in battle (enters bleedout), they have a chance to truly die instead of just getting back up. This chance varies by their injury status.

- **Healthy follower:** 5% chance to die permanently when downed.
- **Lightly Injured follower:** 15% chance to die permanently when downed.
- **Heavily Injured follower:** 40% chance to die permanently when downed.

These values can be adjusted freely from the SKSE in-game menu.  
Setting a value to 0 means they follow vanilla Skyrim rules (never permanently die).

**TR:**  
Bir yoldaş savaşta yere düştüğünde (bleedout), gerçekten ölme ihtimali vardır. Bu ihtimal yaralanma durumuna göre değişir.

- **Sağlıklı yoldaş:** Yere düştüğünde kalıcı olarak ölme ihtimali %5.
- **Hafif yaralı yoldaş:** Yere düştüğünde kalıcı olarak ölme ihtimali %15.
- **Ağır yaralı yoldaş:** Yere düştüğünde kalıcı olarak ölme ihtimali %40.

Bu değerler oyun içi SKSE menüsünden serbestçe değiştirilebilir.  
Değeri 0 yapmak, standart Skyrim kurallarını (kalıcı ölüm yok) uygular.

---

### 🩹 Injury System / Yaralanma Sistemi

**EN:**  
When a follower is downed in battle and survives, they have a 15% chance (configurable) to become **Lightly Injured**. Injuries come in two levels:

#### Light Injury (Hafif Yaralanma)
- Follower receives a **-15% penalty** to attack damage, health/magicka/stamina regeneration, and **-50 armor rating**.
- The follower's downed animation and a **gender-appropriate groan sound** will play:
  - Male followers: male groan sound
  - Female followers: female groan sound
- A notification appears in the top-left: *"[Name] is lightly injured, you should heal them."*
- Optionally, a pop-up message box can appear that you must close manually.

#### Heavy Injury (Ağır Yaralanma)
- If a **Lightly Injured** follower is downed **3 more times** without being healed, they become **Heavily Injured**.
- Follower receives a **-50% penalty** to attack damage, health/magicka/stamina regeneration, and **-150 armor rating**.
- A notification appears: *"[Name] is heavily injured, heal them immediately!"*

**TR:**  
Bir yoldaş savaşta yere düşüp hayatta kaldığında, %15 ihtimalle (ayarlanabilir) **Hafif Yaralı** durumuna girer. Yaralanmaların iki seviyesi vardır:

#### Hafif Yaralanma (Light Injury)
- Yoldaş, saldırı hasarı, can/büyü/dayanıklılık yenilenmesinde **%15 ceza** alır ve **zırhı 50 puan düşer**.
- Yoldaş yaralı animasyonu oynar ve **cinsiyetine uygun inleme sesi** çıkarır:
  - Erkek yoldaşlar: erkek inleme sesi
  - Kadın yoldaşlar: kadın inleme sesi
- Sol üstte bildirim çıkar: *"[Ad] hafif yaralandı, tedavi ettirsen iyi olur."*
- İsteğe bağlı olarak, elle kapatılması gereken bir mesaj kutusu da açılabilir.

#### Ağır Yaralanma (Heavy Injury)
- **Hafif Yaralı** bir yoldaş, iyileştirilmeden **3 kez daha** yere düşerse **Ağır Yaralı** olur.
- Yoldaş, saldırı hasarı, can/büyü/dayanıklılık yenilenmesinde **%50 ceza** alır ve **zırhı 150 puan düşer**.
- Bildirim çıkar: *"[Ad] ağır yaralandı, acilen tedavi ettir!"*

---

### ⚰️ Death Notifications / Ölüm Bildirimleri

**EN:**  
When a follower permanently dies:
- A **notification** appears in the top-left corner: *"[Name] your companion, has died."*
- Optionally, a **message box** appears that you must close by clicking "OK".

Both options can be toggled on/off from the SKSE menu.

**TR:**  
Bir yoldaş kalıcı olarak öldüğünde:
- Sol üstte **bildirim** çıkar: *"[Ad] adındaki yoldaşınız öldü."*
- İsteğe bağlı olarak, "Tamam" butonuna basarak kapatmanız gereken bir **mesaj kutusu** açılır.

Her iki seçenek de SKSE menüsünden açılıp kapatılabilir.

---

### ⛪ Temple Revival & Healing / Tapınak Diriltme ve İyileştirme

**EN:**  
Go to any temple in Skyrim and speak to a **priest NPC**. After the dialogue, the **Life Again menu** becomes unlocked with priest access, allowing you to:

#### Revive Dead Followers (Ölüleri Dirilt)
- Browse all of your dead followers in a list (name, type, level, date of death).
- Select a follower and pay a **Gold fee** (based on their level × multiplier).
- Confirm to revive them in the world.
- **Max Revives Limit (3 Lives):** By default, a follower can only be revived 3 times. If they die a 4th time, their soul cannot be reached and they remain permanently dead. This is configurable in settings.

#### Heal Injured Followers (Yaralıları İyileştir)
- Browse all injured followers (Light or Heavy status shown).
- Select a follower and pay a **Gold fee**:
  - **Light injury:** cheaper cost (level × Light multiplier)
  - **Heavy injury:** more expensive cost (level × Heavy multiplier)
- Confirm to remove all debuffs and restore them to full health.

**TR:**  
Skyrim'deki herhangi bir tapınağa gidin ve bir **rahip NPC** ile konuşun. Diyalogdan sonra **Life Again menüsü** rahip erişimiyle açılır ve şunları yapabilirsiniz:

#### Ölüleri Dirilt (Revive Dead Followers)
- Tüm ölü yoldaşlarınızı bir listede görüntüleyin (isim, tür, seviye, ölüm tarihi).
- Bir yoldaş seçin ve **Altın bedeli** ödeyin (seviye × çarpan).
- Onaylayın ve yoldaş dünyaya geri dönsün.
- **Max Diriltme Sınırı (3 Can):** Varsayılan olarak bir yoldaş sadece 3 kez diriltilebilir. 4. kez ölürse, ruhuna erişilemez ve kalıcı olarak ölü kalır. Bu durum ayarlardan değiştirilebilir.

#### Yaralıları İyileştir (Heal Injured Followers)
- Tüm yaralı yoldaşları (Hafif veya Ağır durumu gösterir) listede görüntüleyin.
- Bir yoldaş seçin ve **Altın bedeli** ödeyin:
  - **Hafif yaralanma:** daha ucuz (seviye × Hafif çarpanı)
  - **Ağır yaralanma:** daha pahalı (seviye × Ağır çarpanı)
- Onaylayın; debuff'lar kalkar ve yoldaşınız tam sağlığına kavuşur.

---

### ⏳ Natural Recovery / Doğal İyileşme

**EN:**  
Lightly injured followers can recover on their own over time without visiting a temple.
- If a **Lightly Injured** follower is **not downed again for 3 in-game days**, they automatically recover.
- This condition is checked instantly whenever you wait, sleep, or fast travel.
- A notification appears: *"[Name] has recovered naturally. ([Name] sağlığına kavuştu.)"*
- **Heavily Injured followers cannot recover naturally** — a temple visit is required.

**TR:**  
Hafif yaralı yoldaşlar tapınağa gitmeye gerek kalmadan zamanla kendiliğinden iyileşebilir.
- **Hafif Yaralı** bir yoldaş **3 oyun günü boyunca yere düşmezse** otomatik olarak iyileşir.
- Bu koşul, beklediğinizde, uyuduğunuzda veya hızlı seyahat ettiğinizde anında kontrol edilir.
- Bildirim gelir: *"[Ad] has recovered naturally. ([Ad] sağlığına kavuştu.)"*
- **Ağır yaralı yoldaşlar doğal olarak iyileşemez** — tapınak ziyareti zorunludur.

---

### 💨 Injury Speed Penalty / Yaralanma Hız Cezası

**EN:**  
Injured followers move slower depending on their injury level:
- **Light Injury:** `-20%` movement speed penalty.
- **Heavy Injury:** `-50%` movement speed penalty (effectively walk-only).

The penalty is automatically removed when the follower is healed.

**TR:**  
Yaralı yoldaşlar, yaralanma seviyelerine göre daha yavaş hareket eder:
- **Hafif Yaralanma:** `-%20` hareket hızı cezası.
- **Ağır Yaralanma:** `-%50` hareket hızı cezası (yürüyerek gider).

Yoldaş iyileştiğinde ceza otomatik olarak kalkar.

---

### 💊 Field Healing with Potions / Sahada İksirle İyileştirme

**EN:**  
You can heal injured followers directly in the field by placing **Cure Disease Potions** in their inventory — no temple visit needed:
- **Light Injury:** 1 Cure Disease Potion is consumed automatically.
- **Heavy Injury:** 3 Cure Disease Potions are required.

The mod checks follower inventories instantly via game events when an item is transferred. Once the required number of potions is detected, they are consumed and the injury is removed with a notification.

**TR:**  
Yaralı yoldaşların **envanterine Hastalık İksiri (Cure Disease Potion)** koyarak sahada iyileştirebilirsiniz — tapınağa gitmeye gerek yok:
- **Hafif Yaralanma:** 1 iksir otomatik harcanır.
- **Ağır Yaralanma:** 3 iksir gereklidir.

Mod, oyun içi eventler vasıtasıyla eşya transferi anında yoldaş envanterlerini anında kontrol eder. Gerekli sayıda iksir bulununca hemen harcanır ve yaralanma bildirimle kaldırılır.

---

### 📊 Follower Dashboard / Yoldaş Durum Paneli

**EN:**  
A dedicated **Dashboard** tab is available in the SKSE menu under **Life Again → Dashboard (Pano)**. It shows a real-time overview of all your followers:

- **Summary bar** at the top: count of Light Injured, Heavy Injured, and Dead followers.
- **Injured Followers Table:** Name, Type, Level, Status (color-coded), Bleedout count, and natural heal countdown.
- **Dead Followers Table:** Name, Type, Level, and date of death.
- Auto-refreshes every ~1 second. Manual **Refresh** button also available.

**TR:**  
SKSE menüsünde **Life Again → Dashboard (Pano)** sekmesinde tüm yoldaşlarınızın gerçek zamanlı durumunu gösteren özel bir panel bulunur:

- **Üstte özet çubuk:** Hafif Yaralı, Ağır Yaralı ve Ölmüş yoldaş sayıları.
- **Yaralı Yoldaşlar Tablosu:** İsim, Tür, Seviye, Durum (renkli), Bleedout sayısı ve doğal iyileşme geri sayımı.
- **Ölmüş Yoldaşlar Tablosu:** İsim, Tür, Seviye ve ölüm tarihi.
- ~1 saniyede bir otomatik güncellenir. Manuel **Yenile** butonu da mevcuttur.

---

## ⚙️ SKSE Menu / SKSE Menüsü

Open the SKSE menu in-game. There are two tabs under **Life Again**:  
Oyun içi SKSE menüsünü açın. **Life Again** altında iki sekme bulunur:

- **General (Genel):** All settings and temple actions.
- **Dashboard (Pano):** Real-time follower status overview.

### General Tab Settings / Genel Sekme Ayarları

| Setting | Description | Açıklama | Default |
|---|---|---|---|
| Gold Multiplier | Gold cost per level for revival | Diriltme için seviye başına altın | 500 |
| Light Injury Cost Multiplier | Cost per level for light injury healing | Hafif yara tedavi çarpanı | 100 |
| Heavy Injury Cost Multiplier | Cost per level for heavy injury healing | Ağır yara tedavi çarpanı | 300 |
| Healthy Death Chance % | Death chance when healthy | Sağlıklı yoldaşın ölme ihtimali | 5 |
| Light Injury Death Chance % | Death chance when lightly injured | Hafif yaralının ölme ihtimali | 15 |
| Heavy Injury Death Chance % | Death chance when heavily injured | Ağır yaralının ölme ihtimali | 40 |
| Show Death Notification | Show top-left message on death | Ölüm bildirimi göster | ON |
| Show Death Message Box | Show pop-up box on death | Ölüm mesaj kutusu göster | OFF |
| Injury Chance % | Chance to get injured after being downed | Yere düşünce yaralanma ihtimali | 15 |
| Show Injury Notification | Show top-left message on injury | Yaralanma bildirimi göster | ON |
| Show Injury Message Box | Show pop-up box on injury | Yaralanma mesaj kutusu göster | OFF |
| Max Revives | Maximum times a follower can be revived (0 = Unlimited) | Maksimum diriltme sayısı (0 = Sınırsız) | 3 |

---

## 🔧 Compatibility / Uyumluluk

**EN:**  
This mod is designed to work alongside popular follower frameworks such as **NFF (Nether's Follower Framework)**. The bleedout/death detection uses `KillImmediate` to bypass framework interference, ensuring followers actually die when the conditions are met.

**TR:**  
Bu mod, **NFF (Nether's Follower Framework)** gibi popüler yoldaş sistemleriyle birlikte çalışacak şekilde tasarlanmıştır. Bleedout/ölüm tespiti, framework müdahalesini atlatmak için `KillImmediate` kullanır ve koşullar sağlandığında yoldaşların gerçekten öldüğünü garanti eder.

---

## 📁 Files & Data / Dosyalar ve Veriler

**EN:**  
The mod stores follower death and injury data persistently in:  
`%APPDATA%\Skyrim Special Edition\SKSE\LifeAgain\followers.json`

This file is automatically created and updated. Dead followers and their injury states survive game restarts.

**TR:**  
Mod, ölü ve yaralı yoldaş verilerini kalıcı olarak şu yerde saklar:  
`%APPDATA%\Skyrim Special Edition\SKSE\LifeAgain\followers.json`

Bu dosya otomatik oluşturulur ve güncellenir. Ölü yoldaşlar ve yaralanma durumları oyun yeniden başlatılsa bile korunur.

---

## 📜 License / Lisans

Copyright (c) 2026 Arif KULPU. All Rights Reserved. — Tüm Hakları Saklıdır.  
See [LICENSE](LICENSE.md) for details.

---

## 👤 Author / Yazar

**Arif KULPU**

---
