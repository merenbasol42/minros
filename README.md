# minros

`minros`, düşük kaynaklı gömülü sistemler için C++17 tabanlı, header-only bir mesajlaşma kütüphanesi geliştirme deneyimidir.
Sade bir wire protokolü üzerinden kanal (CH_ID) tabanlı yayınla/abone ol (pub/sub) iletişimi sağlamayı hedefler.
Güvenilirlik, loglama gibi ek yetenekler çekirdeğe dokunmadan **overlay** olarak eklenir (bkz. [Overlay'ler](#overlayler)).

---

## Tasarım İlkeleri

### Heap yok, sanal dispatch yok

Dinamik bellek tahsisi (`new`, `malloc`) ve sanal fonksiyon (`virtual`) kullanılmaması hedeflenmektedir.
Tüm nesneler derleme zamanında boyutu bilinen statik buffer'larda tutulmakta; bu sayede gömülü sistemlerde bellek tükenmesi ve öngörülemeyen gecikme riskinin önüne geçilmesi amaçlanmaktadır.

### Template parametreleriyle kaynak kontrolü

Statik buffer'lar sabit boyutlu olduğundan farklı projeler farklı miktarda RAM ayırır.
`RawNode`, `Parser`, `Framer`, `Reliable` ve `Node` template parametreleriyle proje ihtiyacına göre ayarlanabilmektedir:

```cpp
// Saf transport: 4 abone, 32 byte'lık frame
minros::RawNode</*MAX_SUBS=*/4, /*MAX_FRAME_DATA=*/32> node;

// Güvenilirlik overlay'i: 2 reliable publisher + 2 reliable subscriber kanalı
minros::overlays::reliability::Reliable</*MAX_PUB=*/2, /*MAX_SUB=*/2> rel{node};

// Tipli yüksek seviye: 4 abone, 32 byte frame, 2 reliable kanal
minros::Node</*MAX_SUBS=*/4, /*MAX_FRAME_DATA=*/32, /*MAX_RELIABLE=*/2> hl;
```

### Donanımdan bağımsızlık

Kütüphane içinde donanıma özgü herhangi bir çağrı bulunmaması hedeflenmektedir.
IO işlevleri kullanıcı tarafından dört callback ile sağlanır; kütüphane bunları transport olarak kullanır:

```cpp
node.transport = {
    .send_bytes = { my_uart_write,     ctx },
    .read_bytes = { my_uart_read,      ctx },
    .get_size   = { my_uart_available, ctx },
    .get_time   = { my_millis,         ctx },
};
```

Şu an UART üzerinde denenmekte olup aynı çekirdek kodun transport callback'leri değiştirilerek UDP gibi farklı ortamlara da taşınabilmesi amaçlanmaktadır.

### Zero-copy, LE ham byte

Wire little-endian'dır; hedef MCU'ların çoğu da LE olduğundan storage'ın bellek
görüntüsü wire byte'larıyla örtüşür ve serileştirme `memcpy` düzeyinde kalır
(bkz. [Wire Protokolü](#wire-protokolü), `utils/endian.hpp`).

### Basitlik

Arayüzün kasıtlı olarak küçük tutulması hedeflenmektedir.
`RawNode::spin_once()` her döngüde çağrılır; gelen baytları işler. Güvenilirlik
kullanılıyorsa ayrıca `Reliable::tick()` çağrılarak timeout/retransmit yürütülür
(`Node::spin_once()` ikisini birlikte yapar).

---

## Mimari

```
┌────────────────────────────────────────────────┐
│  Node (tipli)  ←  RawNode + Reliable sarmalar    │  ← yüksek seviye facade
└───────────────┬────────────────────────────────┘
                │ kullanır (public API)
┌───────────────▼─────────┐     ┌──────────────────────────────┐
│  RawNode (ham byte)     │◄────│  Reliable (overlay)          │
│  publish / subscribe    │     │  seq / ACK / retransmit /    │
│  spin_once()            │     │  duplicate — RawNode'un      │
└────┬──────────────┬─────┘     │  sıradan kullanıcısı (CH249) │
     │              │           └──────────────────────────────┘
  Gönderim       Alım
     │              │
  Framer        Parser  ──→  Broker  ──→  Subscriber callback'leri
     │
  Transport (kullanıcı sağlar)
```

Core (`wireframe`/`framer`/`parser`/`broker`) ve `RawNode` güvenilirlikten habersizdir;
seq diye bir wire alanı yoktur. `Reliable`, RawNode'un public `publish`/`subscribe`
API'sini kullanan bağımsız bir overlay'dir: seq'i payload önekine koyar, ACK'i
normal bir kanaldan (CH249) yollar. `Node` gerektirmez — ham `RawNode` ile de kullanılır.

### Katmanlar

| Dosya | Sorumluluk |
|---|---|
| `core/wireframe.hpp` | Frame formatı sabitleri, CRC-8/SMBUS |
| `core/framer.hpp` | Payload → wire frame dönüşümü (opak head öneki destekli) |
| `core/parser.hpp` | Byte stream → frame (durum makinesi) |
| `core/broker.hpp` | CH_ID bazında callback yönlendirme |
| `raw_node.hpp` | Core'u birleştiren saf ham byte API (`RawNode`) |
| `node.hpp` | `RawNode` + overlay'ler üzerine tipli yüksek seviye sarmalayıcı (`Node`) |
| `interfaces/` | `MsgBase` (CRTP) + mesaj aileleri: `std_msgs/`, `geometry_msgs/` — sabit boyutlu tipler |
| `overlays/` | Çekirdeğe opsiyonel eklenen katmanlar — bkz. [Overlay'ler](#overlayler) |

---

## Overlay'ler

Overlay, çekirdeğe (core + `RawNode`) hiçbir şey eklemeden onun **public
`publish`/`subscribe` API'sini kullanan bağımsız bir katmandır. Ortak sözleşme
(core'a dokunmama, rezerve kanal + opak head öneki, kullanılmazsa sıfır
maliyet) ve rezerve kanal bloğu tablosunun sahibi:
[`minros/overlays/README.md`](minros/overlays/README.md).

- **reliability** (CH249) — stop-and-wait ACK + retransmit ile güvenilir
  teslim; subscriber tarafında dedup + otomatik ACK. Ayrıntı:
  [`reliability-protocol.md`](minros/overlays/reliability/reliability-protocol.md).
- **logging** (CH248) — seviyeli, best-effort log yayını; kaynakta zero-buffer,
  uzun satır otomatik parçalanır. Ayrıntı:
  [`logging-protocol.md`](minros/overlays/logging/logging-protocol.md).
- **parameters** (CH247 REQ / CH246 RES) — düğüm parametrelerini host'tan
  oku/yaz (get/set); kayıtlar derleme-zamanı `constexpr` tablo, RAM tüketmez.
  Ayrıntı: [`parameters-protocol.md`](minros/overlays/parameters/parameters-protocol.md).

---

## Wire Protokolü

```
┌──────────┬─────┬──────────────────────┬─────┐
│ HEADER   │ LEN │ DATA                 │ CRC │
│ 4 byte   │ 1 B │ CH_ID(1) PAYLOAD(n)  │ 1 B │
└──────────┴─────┴──────────────────────┴─────┘
```

- **HEADER**: `{0x6D, 0x72, 0x6F, 0x73}` (`mros`) — senkronizasyon
- **LEN**: DATA uzunluğu (2–249)
- **CRC**: CRC-8/SMBUS, DATA alanının tamamı üzerinden (CH_ID + PAYLOAD)
- Wire formatı little-endian; host dönüşümü `utils/endian.hpp` ile yönetilmektedir

> Core SEQ bilmez. Güvenilir mesajlarda `Reliable`, PAYLOAD'ın önüne 1 baytlık seq
> öneki koyar (`[SEQ][user bytes]`); ACK frame'leri CH249'da `[RESP][CH][SEQ]` taşır.
> Bu, core için opak veridir.

---

## Veri Akışı

### Gönderim

1. Uygulama mesajı (`std_msgs`) `to_bytes()` ile serileştirilir.
2. Reliable ise `Reliable::publish()` seq'i payload önüne ekler ve payload'ın
   **pointer'ını tutar** (kopya yok); ACK bekleniyorsa `false` döner (`can_send`).
3. `Framer::build()` wire frame üretir.
4. `Transport::send_bytes` frame'i iletir.
5. ACK gelmezse `Reliable::tick()` aynı pointer'dan otomatik yeniden gönderir.

### Alım

1. `spin_once()` → `Transport::get_size` + `Transport::read_bytes` ile baytlar parser buffer'ına alınır (zero-copy).
2. `Parser::commit(n)` durum makinesini çalıştırır; frame tamamlanınca broker tetiklenir.
3. `Broker`, `CH_ID` üzerinden ilgili subscriber callback'ini çağırır.
4. Reliable abonelikte `Reliable` wrapper'ı seq önekini ayıklar, duplicate kontrolü
   yapar ve ACK gönderimini otomatik gerçekleştirir.

---

## Kullanım

İki kullanım seviyesi vardır. `Node` tipli mesajlarla (serialize/deserialize otomatik)
çalışan yüksek seviye sarmalayıcıdır; `RawNode` ise doğrudan ham byte ile çalışan çekirdektir.
`Node` kullanmak zorunlu değildir — `RawNode` tek başına oluşturulabilir.

### Yüksek seviye — `Node` (tipli)

```cpp
#include <minros/node.hpp>
#include <minros/interfaces/geometry_msgs/twist.hpp>

minros::Node<> node;

// Transport bağla
node.transport = { ... };

// Reliable publisher — retransmit otonomdur, callback gerekmez.
// Publisher mesajı kendi buffer'ında tutar (retransmit backing).
auto cmd = node.create_publisher<minros::interfaces::geometry_msgs::Twist>(CH_CMD, /*reliable=*/true);

// Reliable subscriber — callback doğrudan tipli mesaj alır
// void on_cmd_received(const minros::interfaces::geometry_msgs::Twist& msg, void* ctx)
node.create_subscription<minros::interfaces::geometry_msgs::Twist>(
    CH_CMD, { on_cmd_received, nullptr }, /*reliable=*/true
);

// Loop içinde
void loop() {
    node.spin_once();                       // parser + reliable tick birlikte
    if (!cmd.publish(twist_msg)) {
        // önceki mesaj hâlâ uçuşta (ACK bekleniyor) — sonra dene
    }
}
```

### Düşük seviye — `RawNode` (ham byte)

`RawNode` tek başına yeterlidir; tipli katmana ihtiyaç yoksa doğrudan kullanılabilir.
Serileştirme/deserileştirme tamamen kullanıcıya aittir.

```cpp
#include <minros/raw_node.hpp>
#include <minros/overlays/reliability/reliable.hpp>

minros::RawNode<> node;
minros::overlays::reliability::Reliable rel{node};   // overlay'i node'a tak (CTAD)

// Transport bağla
node.transport = { ... };

// Unreliable subscriber — callback ham payload alır
// void on_bytes(u8* payload, u8 len, void* ctx)
node.subscribe(CH_TELEM, { on_bytes, nullptr });

// Reliable subscriber (duplicate filtre + otomatik ACK)
// callback seq önekı ayıklanmış kullanıcı verisini alır
rel.subscribe(CH_CMD, { on_cmd_bytes, nullptr });

// Reliable publisher için: buffer ACK'e kadar SABİT kalmalı (Reliable pointer tutar)
static u8 cmd_tx[4];

// Loop içinde
void loop() {
    node.spin_once();   // gelen baytlar
    rel.tick();         // timeout/retransmit

    u8 payload[4] = { 1, 2, 3, 4 };
    node.publish(CH_TELEM, payload, sizeof(payload));   // unreliable

    if (rel.can_send(CH_CMD)) {                         // önceki ACK'lendi mi?
        memcpy(cmd_tx, payload, 4);
        rel.publish(CH_CMD, cmd_tx, sizeof(cmd_tx));    // reliable
    }
}
```

> `Node`, içte bir `RawNode` + `Reliable` tutar ve `to_bytes()` / `from_bytes()`
> çağrılarını bu ham API üzerine ekler; reliable publisher buffer'ını da kendi
> yönetir. İki seviye aynı wire protokolünü paylaşır; karışık kullanılabilir.

---

## Geliştirme / Test Ortamı (`dev/`)

Depo kökündeki `dev/`, kütüphaneyi gerçek donanımda (ESP32-S3) deneymek için
kullanılan, kendi başına tam bir PlatformIO projesidir. minros'u yayınlanmış bir
sürümden değil, **depo kökündeki canlı kaynaktan** symlink ile çeker:

```ini
# dev/platformio.ini
[env]
lib_deps = symlink://..
```

Böylece `minros/` altındaki header'ları düzenle → `dev/` içinde derle → beğenince
tek repoda commit et; ayrı bir test reposu ya da sürüm yükseltme gerekmez.

```bash
cd dev
pio run                 # esp32s3box için derle
pio run -t upload       # cihaza yükle
pio test                # birim + haberleşme testleri
```

VSCode'da IntelliSense için `dev/` klasörünü **doğrudan** aç (PlatformIO
`platformio.ini`'yi orada bulur); gerekirse `pio project init --ide vscode` ile
`c_cpp_properties.json` yenilenir. `tools/` altında host tarafı Python betikleri
(latency/throughput/ACK testleri, minrospy sink) bulunur.

`dev/` yalnızca geliştirme içindir; `library.json`'daki `export.include` sayesinde
PlatformIO ile kütüphaneyi kuran kullanıcılara **gitmez** — onlara yalnızca
`minros/` kaynağı paketlenir.

---

## Sınırlar

- Thread-safe değildir; tipik kullanım tek döngü/thread içindir.
- Reliability modeli basittir: ACK + retry. Tam bir transport protokolü (akış kontrolü, sıralama garantisi) değildir.
- CH_ID ayrımı ve kanal planlaması kullanıcı sorumluluğundadır.
- Parser buffer'ı bir frame'den büyük olamaz; çok büyük payload'lar `MAX_FRAME_DATA` ile kısıtlanmalıdır.

---

## Yol Haritası

### Opsiyonel keşif katmanı

İkili (binary) protokol olmasına karşın birden fazla cihazın birbirini tanıyıp mesajlaşabilmesi için opsiyonel bir keşif mekanizması eklemek hedeflenmektedir.
Her düğümün yayınladığı/abone olduğu kanalları ve mesaj tiplerini duyurabilmesi; bir merkez düğümün (MQTT broker benzeri) bu bilgileri toplayarak çok-noktaya yönlendirme yapabilmesi planlanmaktadır.
Bu özelliğin mevcut çekirdeğe dokunmadan opsiyonel olarak eklenmesi amaçlanmaktadır.

### Diğer

- Birim test kapsamının genişletilmesi (parser edge-case'leri, reliable seq wraparound)
- Hata callback'lerinin standartlaştırılması
