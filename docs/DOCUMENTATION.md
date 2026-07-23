# minros dokümantasyon standardı

Bu dosya, minros'un **tüm** dokümantasyonunun (header yorumları, `.md` tasarım
dokümanları, proje sayfaları) uyduğu tek kaynaktır. Yeni bir özellik eklerken ya
da mevcut bir dokümanı düzenlerken buraya bak. Bir çelişki çıkarsa **bu dosya
kazanır**; ötekiler buna göre düzeltilir.

Hedef: kütüphanenin her köşesinde aynı ses, aynı yapı, aynı kapsam eşiği — bir
kardeş özelliğin dokümanına bakan biri, diğerinde de aynı iskeleti bulsun.

---

## İlke 0 — Üç katman, tek kaynak

Her bilginin **tek bir sahibi** vardır. Diğer yerler o sahibe *link verir*,
içeriği kopyalamaz. Bir tablo veya diyagram iki dosyada birebir tekrarlanıyorsa
biri yanlıştır ve link'e dönüştürülür.

| Katman | Nerede | Neyin sahibi | Neyin sahibi DEĞİL |
|---|---|---|---|
| **API referansı** | Header içi Doxygen | "Nasıl çağrılır": imza, parametre, dönüş, ön/son koşul, sınıf sözleşmesi | Wire byte düzeni, tasarım gerekçesi |
| **Tasarım / protokol** | Özellik başına `.md` | "Neden böyle" + wire kontratı: byte düzeni, op-code, sınırlar, akış | API imza detayı (header'a bırakılır), genel davranış özeti (klasörün `README.md`'sine bırakılır) |
| **Proje** | Her klasörün kendi `README.md`'si (Kural 7), kök `README.md`, `docs/mainpage.md`, `minros/maliyet.md` | "Ne, ilk 5 dakika, mimari harita, kaynak maliyeti" — her klasör kendi seviyesinde anlatır | Özellik-içi protokol detayı (`.md`'ye link) |

**Tekil sahiplik örnekleri (referans):**

- Rezerve kanal bloğu tablosu (249/248/247…) → sahibi
  [`minros/overlays/README.md`](../minros/overlays/README.md). Diğer her yer bu
  tabloya link verir, yeniden yazmaz.
- Tasarım ilkeleri metni (heap yok, template ile kaynak kontrolü, donanımdan
  bağımsızlık) → sahibi `README.md`. `mainpage.md` özetler ve link verir.
- Kaynak/maliyet sayıları → sahibi `minros/maliyet.md`. Başka yerde sayı
  tekrarlanmaz, link verilir.
- Bir overlay'in genel davranış özeti (ör. "reliability ACK gelene kadar
  bekler") → sahibi o overlay'in kendi klasöründeki `README.md` (ör.
  `overlays/reliability/README.md`). `overlays/README.md` yalnızca aile
  tablosunu ve ortak sözleşmeyi taşır, tek tek overlay davranışını tekrarlamaz;
  `*-protocol.md` ise yalnızca wire kontratını taşır.

---

## Kural 1 — Header Doxygen standardı

Doxygen kuruludur; API referansı header yorumlarından üretilir. Bu yüzden
yorumlar yapısaldır, serbest metin değil.

### 1.1 Tag kullanımı

- Doküman bloğu `///` veya `/** ... */` ile yazılır (Doxygen bunları alır). Sıradan
  `//` yorumları Doxygen'e **girmez** — private/iç detay için onları kullan.
- **Her public sınıf, struct, fonksiyon, enum** için `@brief` (tek cümle) zorunlu.
- Anlamı imzadan aşikâr değilse: `@param <ad>`, `@return`.
- Template için: `@tparam <ad>`.
- Sözleşme varsa: `@pre` (ön koşul), `@post` (son koşul), `@warning` (tehlike),
  `@note` (dikkat). Örnek: reliable publisher'ın "buffer ACK'e kadar sabit
  kalmalı" kuralı bir `@warning`'dir.
- İlgili tasarım dokümanına: `@ref <özellik>-protocol.md` ya da ilgili sınıfa
  `@ref minros::...`.

### 1.2 Dosya başı

Her header, `@file` bloğuyla başlar:

- 1 cümle amaç (`@brief`).
- Varsa platform sınırı bir `@warning`/`@note` callout'u (ör. logging'in AVR
  notu, parameters'ın little-endian kısıtı).
- İstenirse `─` kutu-çizimi banner ile geniş bir genel bakış. **Banner yalnızca
  dosya başındadır**; fonksiyon/sınıf üstlerinde banner değil, tag kullanılır.

### 1.3 Kullanım örneği

Sınıf brief'indeki kod örnekleri `@code` / `@endcode` içine alınır (düz metin
değil — Doxygen kod olarak render'lasın):

```cpp
/// @brief Yüksek seviye tipli düğüm (RawNode + reliability sarmalayıcı).
///
/// @tparam MAX_SUBS       Maksimum abonelik sayısı (varsayılan 16).
/// @tparam MAX_FRAME_DATA Frame DATA alanı maksimum uzunluğu (varsayılan 249).
/// @tparam MAX_RELIABLE   Kanal başına güvenilir pub/sub sayısı (varsayılan 8).
///
/// @code
/// minros::Node<> node;
/// node.transport = { ... };
/// auto pub = node.create_publisher<Twist>(ch_id);
/// pub.publish(msg);
/// @endcode
```

### 1.4 Dil ve biçim

- Türkçe, **tam diyakritikli** — "sınıfı", "değil" ("sinifi", "degil" değil).
- Açıklama metni nötr 3. tekil kip. Kod örneği yorumları 2. tekil olabilir
  ("sonra dene").
- Mevcut davranış için kararlı şimdiki zaman ("SET storage'a memcpy edilir"),
  edilgen-temenni ("edilmektedir") değil.

---

## Kural 2 — `.md` tasarım/protokol dokümanı iskeleti

[`parameters-protocol.md`](../minros/overlays/parameters/parameters-protocol.md)
altın standarttır. Wire kontratı olan **her** özelliğin `.md`'si aşağıdaki
iskelete oturur. Bir bölüm o özellik için geçersizse silinmez, başlığı bırakılıp
altına `—` yazılır; böylece kardeş dokümanlar simetrik görünür.

```
# <alan> — <tek cümle amaç>          ← başlık zorunlu; dosya # ile başlar

## Amaç / Ne çözer                    ← 2-3 cümle; varsa ROS analojisi burada
## Tasarım ilkeleri                   ← numaralı; her ilke kendi alt başlığı
## Sınırlar                           ← "> **Sınır N —** ..." callout formatı
## Wire mesajları                     ← byte düzeni + op-code / flags / hata tabloları
## Davranış                           ← durum makinesi / akış / kenar durumları
## Örnek akış                         ← "──" ASCII blok, host ↔ düğüm
## Güvenilirlik / etkileşim (varsa)   ← başka overlay'lerle ilişki
## Kaynak maliyeti (varsa)            ← ya kısa özet ya maliyet.md'ye link
```

### 2.1 Callout formatı

- **Sınır** (hard kısıt): `> **Sınır N — <özet>.** <açıklama>`
- **Not** (dikkat): `> **Not —** ...`
- Numaralandırma dosya içinde tutarlı (Sınır 1, Sınır 2, …).

### 2.2 Wire diyagramı formatı

Byte düzeni ASCII kutu ya da köşeli-parantez notasyonuyla verilir; ikisi de
kabul ama bir doküman içinde **tek** stil:

```
── PARAM_REQ (CH247, host → düğüm) ─────────────────
GET   : [OP=0x01][PARAM_ID]
SET   : [OP=0x02][PARAM_ID][value bytes...]
```

### 2.3 Durum etiketi

Her özellik dokümanının başında (veya ilgili başlıkta) durum açıkça yazılır:
`durum: mevcut` / `durum: planlanan`. Bir özellik "mevcut" ilan edildiği tek yer
kendi `.md`'sidir; README/mainpage bu etikete uyar, kendi başına "planlanan"
demez.

---

## Kural 3 — İsimlendirme ve yerleşim

- Tasarım dokümanı adı: **`<özellik>-protocol.md`** (kebab-case, tek suffix).
  "instruction-", "notu-", "-design" gibi ikinci bir tür **yoktur**. Tasarım
  notu ayrı dosya değil, protokol dokümanının `## Tasarım ilkeleri` bölümüdür.
- **Her klasör tam olarak bir `README.md` içerir** (Kural 7) — yaprak klasörler
  dahil (`core/`, `utils/`, `interfaces/std_msgs/`, `overlays/logging/` gibi).
  O klasörün genel/aile anlatımını taşır; alt klasör veya kardeş `.md` varsa
  Doxygen sayfa ağacında `\subpage` ile onun çocuğu olur.
- Her `.md`, dokümanladığı header ile **aynı klasörde** yaşar.
- Header ↔ doküman eşleşmesi: `foo_protocol.hpp` (snake, C++ dosyası) ↔
  `foo-protocol.md` (kebab, doküman). Dil farkı kabul; ama tek tutarlı çift.

---

## Kural 4 — Kapsam eşiği (hangi birime ne gerekir)

Eksik doküman kadar fazla doküman da hatadır. Eşik:

| Birim türü | Zorunlu | Yeterli |
|---|---|---|
| Her klasör (`core/`, `utils/`, `interfaces/…`, `overlays/…` — yaprak dahil) | Bir `README.md` (Kural 7) + ilgili header'ların Doxygen'i (Kural 1) | — |
| Wire kontratı olan klasör (`overlays/reliability/`, `overlays/logging/`, `overlays/parameters/`, `core/wireframe`) | Yukarıya ek olarak `<özellik>-protocol.md` (Kural 2 iskeleti) | `README.md` genel davranışı anlatır, `.md` yalnızca wire kontratını taşır — biri ötekini tekrarlamaz |
| Tekil mesaj tipi (`std_msgs/primitives.hpp` vb.) | Header Doxygen | Ayrı `.md` gerekmez; ailenin `README.md`'si yeterli |

---

## Kural 5 — Ses, kip, dil (tüm dokümanlar)

- **Dil**: Türkçe, tam diyakritikli.
- **Kip**: mevcut davranış → kararlı şimdiki zaman. Henüz olmayan → `planlanan`
  etiketiyle ayrılır, gövde metni "yapar" der ama başlık "planlanan" taşır.
- **Kişi**: açıklama nötr 3. tekil; kod örneği yorumu 2. tekil.
- **İngilizce terimler**: yerleşik teknik terim (payload, overlay, wire, ACK,
  heap) korunur; ilk geçtiği yerde parantezle Türkçesi verilebilir. Zorlama
  çeviri yapılmaz.

---

## Kural 6 — Çapraz-link ve bakım

- **Yön**: `.md` → header (Doxygen `@ref` / dosya linki); header → `.md`
  (`@ref foo-protocol.md`); README/mainpage → özellik `.md`'si (asla içeriği
  kopyalamaz, link verir).
- **README / mainpage ayrımı**:
  - `docs/mainpage.md` — Doxygen giriş sayfası. Kısa; `@ref`'lerle sınıf/namespace
    ve `.md`'lere yönlendirir. Uzun anlatı buraya girmez.
  - `README.md` — GitHub vitrini. İlk-5-dakika (kurulum, minimal örnek), mimari
    özet, link'ler. Ortak "tasarım ilkeleri" metninin **sahibi** burasıdır;
    mainpage özetleyip link verir.
- **Doküman-kod senkronu**: wire formatı, op-code, kanal ya da public imza
  değişen her commit, ilgili `.md`/Doxygen'i **aynı commit'te** günceller.
- **Yeni özellik akışı**: (1) rezerve kanal seç, `overlays/README.md` tablosuna
  `\subpage` satırı ekle → (2) yeni overlay klasörüne genel davranışı anlatan
  bir `README.md` yaz (Kural 7) → (3) `<özellik>-protocol.md`'yi Kural 2
  iskeletiyle yaz → (4) header'ları Kural 1 ile belgele → (5) `{#id}` ve
  `\subpage` bağlarını Kural 7 ile kur.

---

## Kural 7 — Sayfa hiyerarşisi ve başlıklandırma (Doxygen sidebar)

Doküman sitesinin sol gezinme ağacı, dosya sistemindeki klasör hiyerarşisini
birebir yansıtır. Bu, düz markdown linkiyle değil, Doxygen'in `\subpage`
komutuyla kurulur — aksi halde her `.md` birbirinden bağımsız, düz bir sayfa
listesi olarak görünür.

### 7.1 Her klasör bir sayfa, her sayfa bir kimlik

- `minros/` altındaki her klasör tam olarak bir `README.md` içerir; bu dosya
  o klasörün Doxygen sayfasıdır (Kural 3, Kural 4).
- Her `.md`'nin ilk (`#`) başlığı benzersiz bir `{#id}` taşır. Kimlik klasör
  yoluyla eşleşir: `minros-<yol-tire-ile>` — ör. `overlays/logging/README.md`
  → `{#minros-overlays-logging}`, aynı klasördeki `logging-protocol.md` →
  `{#minros-overlays-logging-protocol}`.

### 7.2 Ebeveyn-çocuk bağı: `\subpage`, düz link değil

Bir alt klasör veya bir klasördeki README dışı `.md`, o klasörün
`README.md`'sinde **`\subpage <id> "<link metni>"`** ile bağlanır. Düz
markdown linki (`[metin](yol)`) sidebar'da hiyerarşi kurmaz — sayfa düz,
kardeş bir sayfa gibi görünür. Bir sayfa yalnızca **tek** bir yerden
`\subpage` ile bağlanmalı (tek ebeveyn).

### 7.3 Başlıkta "minros" tekrarı yok

Sayfa zaten `minros` ağacının bir dalında yaşadığından, başlıkta bunu tekrar
yazma: `# Logging` doğru, `# minros logging` yanlış. Tek istisna kök sayfa
`docs/mainpage.md` (`# minros {#mainpage}`) — proje adını taşıyan gerçek kök.

### 7.4 Prose başlıkları ağacı kirletmez

`docs/Doxyfile`'da `TOC_INCLUDE_HEADINGS = 0` ayarlıdır: bir sayfanın kendi
içindeki `##`/`###` bölüm başlıkları (ör. "Tasarım ilkeleri", "Davranış")
sidebar'da ayrı madde açmaz — yalnızca gerçek `\subpage` sayfaları ağaçta
görünür. Bu yüzden okunabilirlik için sayfa içinde istediğin kadar alt başlık
kullan; sidebar şişmez.

---

## Hızlı kontrol listesi (PR öncesi)

- [ ] Değişen public API'nin `@brief`'i var mı? Yeni `@param`/`@tparam` eklendi mi?
- [ ] Wire formatı değiştiyse ilgili `.md` aynı commit'te güncellendi mi?
- [ ] Yeni `.md` Kural 2 iskeletine ve isimlendirmeye uyuyor mu?
- [ ] Bir tablo/diyagram başka yerden kopyalandı mı? (Kopyalandıysa link'e çevir.)
- [ ] `mevcut` / `planlanan` etiketi doğru ve README ile çelişmiyor mu?
- [ ] Türkçe diyakritik tam mı?
- [ ] Yeni klasör açıldıysa bir `README.md`'si var mı? `{#id}` aldı mı ve
      ebeveyn sayfada `\subpage` ile bağlandı mı? (Kural 7)
