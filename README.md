# Life Again

Life Again, Skyrim Special Edition / Anniversary Edition için SKSE eklentisidir. Aktif yoldaşların savaş sırasında yaralanmasını, yaraların tedavisini ve isteğe bağlı kalıcı ölüm/diriltme sistemini yönetir. Ana oyun sistemleri dışında kalan yara animasyonları ve görünür yara izi entegrasyonları isteğe bağlıdır.

## İçindekiler

- [Gereksinimler](#gereksinimler)
- [Kurulum](#kurulum)
- [Yaralanma sistemi](#yaralanma-sistemi)
- [Tedavi](#tedavi)
- [Ölüm ve diriltme](#ölüm-ve-diriltme)
- [Animasyonlar ve yara izleri](#animasyonlar-ve-yara-izleri)
- [SKSE menüsü ve test laboratuvarı](#skse-menüsü-ve-test-laboratuvarı)
- [Kayıtlar ve ayarlar](#kayıtlar-ve-ayarlar)
- [Geliştirici derlemesi](#geliştirici-derlemesi)
- [Lisans](#lisans)

## Gereksinimler

- Skyrim Special Edition / Anniversary Edition, desteklenen **1.6.x** sürümü
- SKSE64
- Address Library for SKSE Plugins
- SKSE Menu Framework — Life Again menülerini kullanmak için
- Open Animation Replacer (OAR) — yaralanma animasyonları için
- RaceMenu ve **Wounds Overlays For RaceMenu** — görünür yara izi için

SKSE, Address Library, OAR ve RaceMenu entegrasyonları ayrı bileşenlerdir. OAR veya RaceMenu bileşenleri yoksa ana yaralanma ve tedavi sistemi kullanılabilir; ilgili animasyon/yara izi görünümü çalışmaz. Menü bileşeni yoksa SKSE menüleri kullanılamaz.

## Kurulum

### Önerilen: bütünleşik MO2 paketi

Depodaki `distribution/Life Again - Integrated` klasörünü tek bir mod olarak MO2'ye ekleyip etkinleştirin. Bu paket ana eklentiyi, OAR köprüsünü ve yaralanma animasyonlarını birlikte içerir. OAR animasyonlarının görünmesi için ayrıca Open Animation Replacer kurulu ve etkin olmalıdır. Görünür yara izi için RaceMenu ile Wounds Overlays For RaceMenu kurulu ve etkin olmalıdır.

MO2 dışında kurulumda klasör içeriğini oyunun `Data` klasörüne yerleştirin. DLL'ler şu konumda olmalıdır:

```text
Data/SKSE/Plugins/LifeAgain.dll
Data/SKSE/Plugins/LifeAgainOARBridge.dll
```

### Ayrı animasyon paketi

`distribution/Life Again Injury Animations` yalnızca OAR animasyon dosyalarını ve koşullarını içeren ayrı pakettir. Bütünleşik paketi kullanıyorsanız bunu ayrıca etkinleştirmeyin; aynı animasyon dosyalarını iki moddan yüklemek çakışma ve gereksiz dosya önceliği sorunları yaratabilir. Ayrı paketi seçerseniz ana Life Again DLL'lerini başka bir mod kurulumundan yükleyin.

## Yaralanma sistemi

Yalnızca modun aktif takipçi olarak tanıdığı yoldaşlar yaralanma kontrolüne alınır. Geçerli bir savaş vuruşundan sonra aynı yoldaş için kısa bir bekleme süresi uygulanır; böylece tek bir saldırı dizisinin art arda çok sayıda yara üretmesi önlenir. Her vuruş yaralanma oluşturmaz.

Yaralanmalar iki şiddet grubuna ayrılır:

**Hafif yaralar**

- Sıyrık / aşınma
- Kesik
- Hafif yanık
- Ezik / morluk

**Ağır yaralar**

- Derin kesik / açılma
- Delici yaralanma
- Parçalı / ezikli yara
- Ciddi yanık

Varsayılan yaralanma olasılıkları hafif yara için **%5**, ağır yara için **%1**'dir. Önce ağır yara olasılığı kontrol edilir; ağır yara oluşmazsa hafif yara kontrol edilir. Değerler SKSE menüsünden değiştirilebilir ve ağır yara olasılığı hafif yaraya kıyasla sınırlanır. Bunlar her vuruş için olasılıktır; belirli bir savaşta yara oluşacağını garanti etmez.

Yaralanma şiddetine göre yoldaşın saldırı gücü, hareket hızı, dayanıklılık/magicka yenilenmesi ve zırhı azalır. Yeni yara oluştuğunda yaralı tepki sesi ve uygun animasyon olayı tetiklenebilir. Etkiler tedavi edilince kaldırılır. Animasyon seçimi vücudun tam yara konumuna değil, yaralanmanın şiddetine dayanır.

## Tedavi

### İksirle tedavi

Life Again, oyundaki **Hastalık Tedavi İksiri (Cure Disease Potion)** öğesini kullanır. Yaralı yoldaşın envanterinde en az bir iksir varsa bir tane içer ve rastgele seçilen tek bir yara iyileşir. Birden fazla yarası varsa kalan iksirleri de kullanıldıkça yaralar tek tek iyileşir. Healing menüsünden oyuncunun envanterindeki iksirlerle tüm yaraları tek seferde tedavi edebilirsin; bu işlem her hafif yara için **1**, her ağır yara için **3** iksir ister.

### Altınla tedavi

Healing menüsünde yaraları altın karşılığında tedavi etme seçeneği bulunur:

- Her hafif yara: **100 altın**
- Her ağır yara: **250 altın**

### Elle tedavi

Otomatik rahip taraması ve takipçinin rahibe yaklaşıp kendi altınını harcayarak tedavi olması kaldırıldı. Tedaviyi SKSE menüsündeki **Healing (Tedavi)** bölümünden başlatabilirsin. İksir satın alımını Life Again yapmaz; bu işi Follower Unbound yürütür. Yeterli iksiri taşıyan takipçi bunları kullanarak yaralarını otomatik iyileştirebilir. Elle altınla tedavi seçeneği menüde kullanılabilir.

Yara uygulama ve iksir tedavisi adımları `Documents\My Games\Skyrim Special Edition\SKSE\LifeAgain_Treatment.log` dosyasına yazılır. Yara izi/overlay uygulama adımları `LifeAgainOARBridge.log` dosyasına yazılır. Takipçi iksir tedavisinde oyun motorunun `Actor::DrinkPotion` işlevi kullanılır; bu, NPC'nin iksiri kendi envanterinden içmesini ve Skyrim'in standart içme davranışını tetikler. King Core kaydı, tedavi iksirinin Follower Unbound tarafından satılmasını veya envanter temizliğinde silinmesini önler; iksir satın alma işini Life Again yapmaz.

### Dinlenerek iyileşme

Hafif yaralar, oyun takviminde geçen her **3 günde bir rastgele tek bir hafif yara** iyileşecek şekilde doğal toparlanır. Yaralanma menüsü her takipçi için bir sonraki doğal iyileşmeye kalan oyun günü sayısını gösterir. Ağır yaralar kendiliğinden iyileşmez. Dinlenme ve uyku, oyun saatini ilerlettiği ölçüde bu süreye katkı sağlar. Follower Unbound oyuncunun bekleme/uyku bitişini algılar ve takipçilerin dinlendiğini King Core üzerinden Life Again'e bildirir; Life Again iyileşme hesabını bu olayda günceller. İyileşme ilerlemesi kayıt dosyasına yazılır. Bu bağlantı için güncel King Core, Follower Unbound ve Life Again DLL'lerinin birlikte kurulu olması gerekir.

Takipçinin kendi kullandığı her tedavi iksiri rastgele **tek bir yarayı** iyileştirir; başka yaralar varsa doğal iyileşme sayacı onlar için devam eder. Son hafif yara iksirle iyileşirse doğal iyileşme sayacı gösterilmez. Healing menüsünden tüm yaralar için gerekli iksirleri kullanarak tam tedavi seçildiğinde yaralanma kaydı ve sayaç tamamen temizlenir.

## Ölüm ve diriltme

Yoldaşın yere düşüp iyileşmeden ölmesi durumunda kalıcı ölüm ihtimali sağlık/yaralanma seviyesine göre hesaplanır. Varsayılan ihtimaller sağlıklı durumda **%5**, hafif yaralı durumda **%15**, ağır yaralı durumda **%40**'tır. Değerler SKSE menüsünden ayarlanabilir; **%0**, ilgili seviyede kalıcı ölüm ihtimalini kapatır.

Diriltme menüsü rahiple etkileşim üzerinden açılır. Varsayılan altın bedeli yoldaşın seviyesinin **500 katıdır**. Ayarlardan ruh taşı şartı açılabilir; yalnızca dolu ruh taşları sayılır. Altın ve ruh taşı şartları birlikte etkinse ikisi de gerekir. `MaxRevives` varsayılan olarak 3'tür; 0 sınırsız diriltme anlamına gelir. Diriltilen yoldaş oyuncunun yakınına getirilir ve eski yara kayıtları temizlenir.

## Animasyonlar ve yara izleri

`LifeAgainOARBridge.dll`, ana DLL'in sunduğu yaralanma durumunu OAR koşullarına aktarır. Animasyon paketi hafif ve ağır yaralanma için uygun bekleme/hareket animasyonlarını seçer. Yaralı animasyonların diğer animasyon setleri tarafından bastırılmaması için önceliği yüksektir. Bu animasyonlar yara türünü veya vücuttaki kesin yara yerini göstermez; şiddet seviyesine göre uygulanır.

Bridge, yaralanma sürdüğü müddetçe RaceMenu yara overlay'ini görünür kılar. Bunun yoldaşlara uygulanabilmesi için RaceMenu `skee64.ini` dosyasındaki `[Overlays]` bölümünde `bPlayerOnly=0` olmalıdır. Tüm yaralar tedavi edildiğinde, ölüm temizliği yapıldığında veya yoldaş diriltildiğinde overlay kaldırılır. Yara izi için RaceMenu ve Wounds Overlays For RaceMenu'nün içerik dosyaları gerekir; Life Again bu dokuların yerine geçmez.

Animasyon paketinde kullanılan bazı HKX dosyaları yerel Wounds animasyon kurulumundan alınmıştır. Bu kaynak dosyaları özgün yazarın izni olmadan yeniden dağıtmayın. Bütünleşik paketteki DLL ve animasyon dosyaları, aynı adlı ayrı animasyon paketini tekrar kurmanızı gerektirmez.

## SKSE menüsü ve test laboratuvarı

SKSE menüsünde **Settings**, **Healing**, **Revival**, **Dashboard** ve **Injury Test Lab** bölümleri bulunur. Settings bölümünden yaralanma ve kalıcı ölüm olasılıkları gibi değerleri değiştirebilirsiniz.

Injury Test Lab, oyun içinde yakındaki aktif insansı yoldaşlarda sistemi denemek için kullanılır. Aynı hücrede, oyuncunun 2500 birim yakınındaki canlı ve aktif takipçiler listelenir; hayvan yoldaşlar ve diğer NPC'ler bu sisteme dahil değildir. Yoldaş seçip sekiz yara türünden birini uygulayabilir; envanterine tedavi iksiri veya altın ekleyebilir; taşıdığı kaynaklarla tedaviyi sınayabilir ya da test yarasını temizleyebilirsiniz. Test yoldaşına uygulanan durum kayıt dosyasına yazılabilir; deneme bitince laboratuvardaki temizleme seçeneğini kullanın.

## Kayıtlar ve ayarlar

Ölü yoldaşlar, yaralanma sayıları ve diriltme kayıtları SKSE co-save dosyasında saklanır ve ilgili Skyrim kaydıyla birlikte taşınır. Ayarlar şu dosyada tutulur:

```text
Documents/My Games/Skyrim Special Edition/LifeAgain/settings.json
```

Önceki sürümlerde kaydedilmiş yaralı yoldaş kayıtları yeni yara sayısı biçimine yükseltilir. Eski kayıtta yara türü bilgisi bulunmuyorsa, kayıtlı şiddete uygun varsayılan bir yara türü oluşturulur. Ayar dosyanızı değiştirmeden önce yedeklemek iyi olur.

## Geliştirici derlemesi

Proje CMake ve C++23 kullanır. Derleme ortamında Skyrim/SKSE CommonLibSSE-NG bağımlılıkları, CMake, Visual Studio C++ araçları ve `VCPKG_ROOT` tanımlı olmalıdır. King Core başlıkları varsayılan olarak `../King Core/include` konumunda aranır; farklıysa `KING_CORE_INCLUDE_DIR` ayarlayın.

```powershell
cmake --preset release
cmake --build build --config Release
```

İsteğe bağlı OAR köprüsünü derlemek için CMake yapılandırmasında `-DBUILD_LIFEAGAIN_OAR_BRIDGE=ON` kullanın. Hazır MO2 paketi `distribution/Life Again - Integrated` altında bulunur.

## Lisans

Life Again için telif ve kullanım koşulları [LICENSE.md](LICENSE.md) dosyasındadır. Üçüncü taraf animasyon ve overlay içerikleri kendi yazarlarının lisanslarına tabidir.
