# minros {#mainpage}

**minros**, düşük kaynaklı gömülü sistemler için C++17 tabanlı, **header-only** bir
mesajlaşma kütüphanesidir. Sade bir wire protokolü üzerinden kanal (`CH_ID`) tabanlı
**yayınla/abone ol** (pub/sub) iletişimi sağlar. ROS'un tanıdık zihinsel modelini
(düğüm, kanal, mesaj) MCU'lara taşır — ama string, heap ve sanal dispatch olmadan.

Güvenilirlik, loglama, parametreler gibi ek yetenekler çekirdeğe dokunmadan
**overlay** olarak eklenir: her overlay yalnızca `RawNode`'un public `publish`/
`subscribe` API'sini kullanan bağımsız bir katmandır.

## Tasarım ilkeleri

- **Heap yok, sanal dispatch yok.** `new`/`malloc` ve `virtual` kullanılmaz; tüm
  nesneler derleme-zamanı boyutu bilinen statik buffer'larda yaşar. Böylece bellek
  tükenmesi ve öngörülemeyen gecikme riski ortadan kalkar.
- **Template ile kaynak kontrolü.** Buffer boyutları (`MAX_SUBS`, `MAX_FRAME_DATA`,
  `MAX_RELIABLE` …) template parametreleridir; her proje yalnızca ihtiyacı kadar RAM
  ayırır.
- **Donanımdan bağımsız.** Kütüphane içinde donanıma özgü çağrı yoktur; IO, kullanıcı
  tarafından verilen dört callback (transport) üzerinden yapılır.
- **Zero-copy, LE ham byte.** Wire little-endian; hedef MCU'lar da LE olduğundan
  storage'ın bellek görüntüsü = wire byte'ları → serileştirme maliyeti minimum.

```cpp
// Saf transport: 4 abone, 32 byte'lık frame
minros::RawNode</*MAX_SUBS=*/4, /*MAX_FRAME_DATA=*/32> node;

// Tipli yüksek seviye facade: + 2 reliable kanal
minros::Node</*MAX_SUBS=*/4, /*MAX_FRAME_DATA=*/32, /*MAX_RELIABLE=*/2> hl;
```

## Mimari: çekirdek + overlay'ler

Çekirdek (`RawNode` ve altındaki framer/parser/broker) yalnızca opak datagramları
taşır. Üstüne oturan overlay'ler protokol yetenekleri ekler ama çekirdeği hiç
değiştirmez:

| Overlay | Kanal | Ne yapar |
|---|---|---|
| **reliability** | 249 (ACK) | Stop-and-wait ACK + retransmit ile güvenilir teslim |
| **logging** | 248 | Seviyeli, best-effort log yayını |
| **parameters** | 247 / 246 | Düğüm parametrelerini host'tan oku/yaz (get/set) |

## Nereden başlamalı

- **Overlay mimarisi ve sözleşmesi:** @ref minros::overlays "overlays"
- **Parametre protokolü:** [parameters-protocol.md](parameters-protocol.md)
- **Loglama protokolü:** [logging-protocol.md](logging-protocol.md)
- **Güvenilirlik protokolü:** [reliability-protocol.md](reliability-protocol.md)
- **Kaynak/maliyet notları:** [maliyet.md](maliyet.md)

Tüm sınıf ve namespace referansı için üstteki arama kutusunu ya da soldaki gezinme
ağacını kullan.
