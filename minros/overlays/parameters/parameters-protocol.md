# minros parameters overlay — wire protokolü

Parametre (parameter) overlay'i, düğümlerin çalışma-zamanı yapılandırma
değerlerini (kazanç, eşik, mod vb.) host tarafından **okunup/yazılabilir** hâle
getirir. ROS parametrelerinin gömülü-dostu, string'siz karşılığıdır.

## Tasarım ilkeleri

### 1. Wire'da sayısal ID, isim yok

Parametreler wire üzerinde **sayısal `u8` kimlikle** (`PARAM_ID`) taşınır — tıpkı
kanallar gibi. Gömülü tarafta string tutmak (RAM/flash maliyeti, her erişimde
`strcmp`, değişken boyutlu frame) protokolün sabit-boyut / zero-alloc felsefesine
ters düşer. Model **CANopen object dictionary**'ye benzer: wire'da indeks,
insan-okur isimler yalnızca host/offline tarafta.

- **Wire**: `PARAM_ID` (`u8`) — 0..255, bir MCU için fazlasıyla yeterli.
- **İsimler**: opsiyonel, yalnızca host (minrospy) tarafında `{id: "kp"}` eşlemesi.
  Çekirdek protokol string içermez.

### 2. Tip wire'da TAŞINMAZ — değer, ham LE byte bloğudur

Pub/sub kanalları opak datagramdır; alıcı bir kanalın tipini konvansiyonla bilir.
Parametre de aynı ilkeyi izler: bir `PARAM_ID`'nin hangi tip olduğu wire'da
**gönderilmez**, host'ta **statik bir manifest**te (koordinasyon / codegen) yaşar.
Böylece frame'den `[FAMILY_ID][TYPE_ID]` çifti ve düğümden tip-eşleştirme yolu
tamamen kalkar.

Değer, tipin sabit-boyutlu **little-endian** byte gösterimidir. minros wire'ı LE,
hedef MCU'lar (Cortex-M/ESP32/AVR) LE olduğundan **storage'ın bellek görüntüsü =
wire byte'ları**. Bu sayede düğüm serileştirme yapmaz:

- **VALUE**: `[OP][ID]` öneki + storage payload'ı **doğrudan** yayılır (zero-copy).
- **SET**: gelen wire byte'ları storage'a **doğrudan** `memcpy` edilir.

> **Sınır 1 — yalnızca little-endian hedef.** Ham-byte yolu big-endian'da
> geçerli değildir; kayıt sırasında `static_assert(NATIVE_IS_LITTLE)` ile derleme
> zamanı reddedilir. (minros'un diğer katmanları BE-taşınabilirdir; parametre
> overlay'i, maliyeti düşürmek için bilinçle LE'ye bağlıdır.)
>
> **Sınır 2 — yalnızca sabit boyutlu, padding'siz POD.** `is_trivially_copyable`
> ve `sizeof(MsgT) == MsgT::SIZE` derleme zamanı zorlanır. Değişken uzunluklu
> (string, dizi) ve padding'li tipler kapsam dışıdır.

Kompozit tipler (ör. `PidGains`) tek parametre olarak yazılınca `kp/ki/kd` **aynı
anda** güncellenir → **atomiklik**; üç ayrı SET arasındaki tutarsız ara durum
oluşmaz.

### 3. İki kanal: REQ / RES

Parametre trafiği **iki ayrı kanal** kullanır — istek ve yanıt için birer tane:

| Kanal | Ad | Yön | İçerik |
|------:|-----|-----|--------|
| **247** | `PARAM_REQ` | host → düğüm | GET, SET |
| **246** | `PARAM_RES` | düğüm → host | VALUE, ERR |

Bu ayrım şart, çünkü **reliability overlay'i kanal başına tek publisher varsayar**
(stop-and-wait, kanal başına tek `seq`/ACK uzayı). Her kanalda tek publisher
olunca reliability temiz çalışır. Yön kanalla, işlem op-code ile ayrılır.

Rezerve kanal bloğu (overlay'ler, kullanıcı kanallarıyla çakışmasın diye üstten):

| Kanal | Overlay |
|------:|---------|
| 249 | reliability ACK |
| 248 | logging |
| 247 / 246 | **parameters** (REQ / RES) |

## Wire mesajları

Her frame'in payload'ının ilk baytı **op-code**'dur; logging'deki `FLAGS` gibi
core'un anlamını bilmediği opak bir head önekidir.

```
── PARAM_REQ (CH247, host → düğüm) ────────────────────────────────
GET   : [OP=0x01][PARAM_ID]
        Verilen parametrenin güncel değerini ister.

SET   : [OP=0x02][PARAM_ID][value bytes...]
        Değeri yazar. value, tipin LE byte gösterimidir (tip host'ça bilinir).

── PARAM_RES (CH246, düğüm → host) ────────────────────────────────
VALUE : [OP=0x03][PARAM_ID][value bytes...]
        GET'e yanıt ve/veya başarılı SET onayı.

ERR   : [OP=0x04][PARAM_ID][CODE]
        İşlem reddedildi (aşağıdaki CODE tablosu).
```

### Op-code'lar

| OP | Ad | Kanal |
|----:|-----|-------|
| 0x01 | GET   | PARAM_REQ (247) |
| 0x02 | SET   | PARAM_REQ (247) |
| 0x03 | VALUE | PARAM_RES (246) |
| 0x04 | ERR   | PARAM_RES (246) |

### Hata kodları (ERR CODE)

| CODE | Anlam |
|-----:|-------|
| 0x00 | UNKNOWN_ID  — bu ID'de kayıtlı parametre yok |
| 0x01 | READ_ONLY   — parametre salt-okunur, yazılamaz |
| 0x02 | BAD_LENGTH  — value bytes uzunluğu tipin SIZE'ından kısa |
| 0x03 | REJECTED    — event handler (BEFORE_SET) değişikliği reddetti |

> Tip wire'da taşınmadığı için `TYPE_MISMATCH` diye bir hata yoktur: aynı boyutlu
> yanlış tip düğümde ayırt edilemez (pub/sub'ın kabul ettiği ödünün aynısı).
> Doğru tipi göndermek host manifest'inin sorumluluğundadır.

## Düğüm tarafı davranışı (registry)

Kayıtlar **çalışma-zamanı değil, derleme-zamanı** bir `constexpr` tablodur.
Tablo `.rodata`'ya (flash) yerleşir → registry RAM tüketmez. Boyut giriş
sayısından türer; "MAX_PARAMS" tavan tahmini ve boşa yatan slot yoktur.

```cpp
constexpr auto TABLE = parameters::table(
    parameters::rw<0>(&gains),     // okunur/yazılır (kompozit → atomik)
    parameters::rw<1>(&thresh),    //
    parameters::ro<2>(&uptime));   // salt-okunur

parameters::Params params{node, TABLE};   // PARAM_REQ'e abone olur (CTAD)
```

Her giriş yalnızca `{id, size, flags, storage*}` tutar (32-bit ABI'de **8 byte**):
tip etiketi yok, serileştirme fonksiyon pointer'ı yok. `rw<ID>/ro<ID>` içindeki
`static_assert`'ler (LE, trivially-copyable, padding'siz) ham-byte yolunu
güvenceye alır. `MsgT` derlemeden sonra "buharlaşır".

> **Kısıt:** `constexpr` tablo, storage adreslerinin sabit-ifade olmasını
> gerektirir → parametre değişkenleri **statik ömürlü** (global/static) olmalı.

- **SET** (CH247): `id` aranır → read-only/uzunluk kontrol → event `BEFORE_SET`
  (reddedilebilir) → storage'a `memcpy` → event `AFTER_SET` → onay **VALUE** (CH246).
- **GET** (CH247): storage payload'ı doğrudan **VALUE** (CH246) olarak yayılır.
- Uyuşmazlıkta ilgili **ERR** (CH246) yollanır.

### Event handler (overlay başına tek delegate)

Doğrulama ve değişim-bildirimi **tek bir** callback'te birleşir (param başına
değil → sıfır ek per-entry maliyet). SET akışında iki faz verilir:

- **BEFORE_SET**: önerilen değer (henüz yazılmadı). `false` → `REJECTED`,
  storage'a dokunulmaz.
- **AFTER_SET**: değer yazıldıktan sonra bildirim (dönüş yok sayılır).

İmza `bool(u8 id, Event ev, const u8* bytes, u8 len, void* ctx)`; `bytes` LE ham
değerdir. Değeri okumak için `reinterpret_cast` **kullanma**, `MsgT::from_bytes`
kullan: `bytes` frame buffer'ının içine işaret eder ve hizalı olma garantisi
yoktur (Cortex-M0'da unaligned erişim fault verir); `from_bytes` memcpy tabanlı
olduğundan hem hizalama hem endian güvenlidir. Tip `id`'ye göre (host manifest'iyle
aynı anlaşma) bilinir. Aralık/clamp mantığı burada, tipin statik olarak bilindiği
kullanıcı kodunda yaşar.

## Güvenilirlik

Yapılandırma verisi kaybolmamalı; bu yüzden SET/VALUE **reliability overlay'i**
(CH249 ACK) üzerinden güvenilir taşınabilir. İki kanallı tasarım tam da bunu
mümkün kılar: `PARAM_REQ`'te tek publisher host, `PARAM_RES`'te tek publisher
düğüm → her kanal reliability'nin tek-publisher sözleşmesine uyar. Overlay yine
de reliability'den bağımsızdır; ikisi de `RawNode`'un pub/sub API'sini kullanan
ayrı katmanlardır.

## Örnek akış

`thresh` bir `Float32`, `pid` bir `PidGains` parametresi. Tip wire'da yok:

```
── Float32 oku/yaz ────────────────────────────────────────────────
Host → 247:  GET   id=1                     [01][01]
Düğüm→ 246:  VALUE id=1 val=1.5             [03][01][00 00 C0 3F]

Host → 247:  SET   id=1 val=2.0             [02][01][00 00 00 40]
Düğüm→ 246:  VALUE id=1 val=2.0             [03][01][00 00 00 40]

── PidGains atomik yaz (kp,ki,kd tek seferde) ─────────────────────
Host → 247:  SET   id=7 <12 byte>           [02][07][<12 byte>]
Düğüm→ 246:  VALUE id=7 <12 byte>           [03][07][<12 byte>]

── Hatalar ────────────────────────────────────────────────────────
Host → 247:  SET   id=9 (kayıtsız)          [02][09][...]
Düğüm→ 246:  ERR   id=9 UNKNOWN_ID          [04][09][00]

Host → 247:  SET   id=2 (salt-okunur)       [02][02][...]
Düğüm→ 246:  ERR   id=2 READ_ONLY           [04][02][01]
```
