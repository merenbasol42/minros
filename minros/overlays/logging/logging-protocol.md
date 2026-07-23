# Logging overlay — seviyeli, best-effort log yayını {#minros-overlays-logging-protocol}

durum: mevcut

## Amaç / Ne çözer

Logging overlay'i, düğümün (slave) metin/binary log satırlarını rezerve log
kanalından (CH248, [`overlays/README.md`](../README.md) kanal tablosu) host'a
yayınlamasını sağlar — ROS'taki `rosout`'un gömülü-dostu karşılığı. Pratikte
slave yayınlar, host (minrospy) sink olur; nokta-nokta link, tek yönlü akış.
Uzun satırlar küçük frame'lere otomatik parçalanır; sink parçaları güvenle
birleştirir.

## Tasarım ilkeleri

### 1. FLAGS opak bir head önekidir

Logging katmanı, kullanıcı text'inin önüne 1 baytlık `FLAGS` öneki koyar
(`PAYLOAD = [FLAGS][text/bytes...]`). Core FLAGS/level bilmez; öneki yalnızca
`LogSink` ayıklar — reliability'nin SEQ önekiyle aynı desen.

### 2. Açık FLAGS başlığı, `\0` sentinel değil

Frame'ler zaten LENGTH ile sınırlıdır; bir parçanın bittiği bilinir. Sentinel
yalnızca "birden fazla frame'e taşıyor" sinyali için gerekirdi ama
sentinel-tarama:

- binary-safe değildir (text içindeki 0x00 bölünmeyi bozar),
- unreliable kanalda düşen parçayı tespit edemez → iki log'un parçalarını
  sessizce birleştirip bozuk satır üretebilir.

Açık FLAGS başlığında `LAST` biti bitişi, `SEQ4` süreklilik/kayıp tespitini verir.

### 3. Publisher zero-buffer, reassembly yalnızca sink'te

`Logger` (slave) hiçbir buffer tutmaz: log fire-and-forget'tir, string literal
zaten flash'ta yaşar ve framer oradan doğrudan okur (zero-copy). Yeniden
birleştirme buffer'ı (`REASM_BUF`) yalnızca `LogSink`'te (host) vardır; slave
onu instantiate etmez, RAM ödemez.

### 4. Best-effort — reliability'ye sokulmaz

Log kanalında ACK/retransmit yoktur; log kaybolabilir ve bu kabul edilebilir
davranıştır. Loglama asla ana akışı bloklamaz. Kaynakta `min_level` filtresi,
eşik altı çağrıları wire'a hiç dokundurmadan döndürür.

## Sınırlar

> **Sınır 1 — memory-mapped flash varsayımı (AVR'de çalışmaz).** Tasarım,
> flash'ı memory-mapped olan hedefleri varsayar (Cortex-M/STM32, ESP32). Klasik
> AVR (Harvard) mimarisinde flash memory-mapped değildir; string literal'ler ya
> RAM'e kopyalanır ya da PROGMEM + `pgm_read_byte` gerektirir. Framer'ın düz
> `payload[i]` okuması bunu yapamayacağından flash'tan zero-copy log (ve
> parçalama) AVR'de olduğu gibi çalışmaz. AVR desteği gerekiyorsa PROGMEM-aware
> ayrı bir okuma yolu tasarlanmalıdır.

> **Sınır 2 — teslim garantisi yoktur.** Kanal best-effort'tur: düşen parça
> yeniden istenmez, satır sink'te atılır (`dropped` sayacı artar). Kaybolmaması
> gereken veri log kanalıyla değil, reliability overlay'iyle taşınır.

## Wire mesajları

```
── LOG (CH248, düğüm → host) ──────────────────────────────────────
LOG   : [FLAGS][text/bytes...]
        Bir log satırının tek parçası (veya tamamı).
```

### FLAGS bit yerleşimi (1 byte)

| Bit | Alan | Anlam |
|-----|------|-------|
| 0 | `LAST` | 1 → log'un son (veya tek) parçası |
| 1..3 | `LEVEL` | Seviye 0..4; her parçada taşınır → parse tekdüze |
| 4..7 | `SEQ4` | 0..15 dönen parça sayacı; kayıp/atlama tespiti |

### Seviyeler (LEVEL)

| Değer | Ad |
|------:|-----|
| 0 | DEBUG |
| 1 | INFO |
| 2 | WARN |
| 3 | ERROR |
| 4 | FATAL |

Tek frame'e sığan log: `LAST=1, SEQ4=0`.

## Davranış

**Parçalama (publisher)** — `FRAME_DATA` küçük kurulabildiği (ör. 32) için uzun
satır tek frame'e sığmayabilir. `Logger`, metni `CHUNK = FRAME_DATA - 2`
(CH_ID + FLAGS) boyutlu parçalara böler:

- İlk parça `SEQ4=0` ile başlar, her parça +1 taşır (16'da sarar).
- Son parça `LAST=1` taşır.
- `min_level` altındaki çağrılar wire'a hiç dokunmaz.
- Publisher tarafında birleştirme buffer'ı yoktur — parçalar framer'ın kendi TX
  buffer'ından yayılır (zero-copy).

**Yeniden birleştirme (sink)** — `LogSink` parçaları `REASM_BUF` içinde toplar:

- Yeni mesaj `SEQ4=0` ile başlar; ortadan (`seq != 0`) yakalanırsa parça atılır.
- Beklenen `SEQ4` gelmezse yarım satır atılır (`dropped++`).
- Buffer taşarsa satır atılır (`dropped++`).
- `LAST` görülünce tam satır `cb(level, msg, len)` ile teslim edilir.

API imzaları ve template parametreleri için [`logger.hpp`](logger.hpp).

## Örnek akış

`FRAME_DATA = 10` → `CHUNK = 8` varsayımıyla:

```
── Tek parçalı log ────────────────────────────────────────────────
Düğüm→ 248:  INFO "hello"                   [03]["hello"]
             FLAGS=0x03 → SEQ4=0, LEVEL=INFO(1), LAST=1

── Parçalı log (12 byte, CHUNK=8) ─────────────────────────────────
Düğüm→ 248:  INFO "temperat"                [02]["temperat"]
             FLAGS=0x02 → SEQ4=0, LEVEL=INFO, LAST=0
Düğüm→ 248:  INFO "ure!"                    [13]["ure!"]
             FLAGS=0x13 → SEQ4=1, LEVEL=INFO, LAST=1
Host      :  cb(INFO, "temperature!", 12)

── Parça kaybı ────────────────────────────────────────────────────
Düğüm→ 248:  [02][parça 0]
Düğüm→ 248:  [12][parça 1]  ✕ (kayıp)
Düğüm→ 248:  [23][parça 2]  ← beklenen SEQ4=1 gelmedi
Host      :  satır atılır (dropped++), bozuk satır ÜRETİLMEZ
```

## Güvenilirlik / etkileşim

Log kanalı reliability overlay'ine bilinçli olarak sokulmaz (bkz. Tasarım
ilkesi 4 ve [`reliability-protocol.md`](../reliability/reliability-protocol.md)):
ACK beklemek log yolunu bloklar, retransmit backing'i slave'e RAM maliyeti
bindirirdi. Kayıp tespiti sink tarafında `SEQ4` sürekliliğiyle yapılır.

## Kaynak maliyeti

Publisher (`Logger`) buffer tutmaz (yalnızca node pointer'ı + `min_level`);
reassembly maliyeti (`REASM_BUF`, varsayılan 128 byte) yalnızca host tarafındaki
`LogSink`'tedir. Genel kaynak tablosu: [`maliyet.md`](../../maliyet.md).
