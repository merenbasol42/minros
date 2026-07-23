# minros {#mainpage}

*(**M**inros **I**s **N**ot **ROS** — Türkçesi: "ROS değildir".)*

**minros**, düşük kaynaklı gömülü sistemler için C++17 tabanlı, **header-only** bir
mesajlaşma kütüphanesidir. Sade bir wire protokolü üzerinden kanal (`CH_ID`) tabanlı
**yayınla/abone ol** (pub/sub) iletişimi sağlar. ROS'un tanıdık zihinsel modelini
(düğüm, kanal, mesaj) MCU'lara taşır — ama string, heap ve sanal dispatch olmadan.

Güvenilirlik, loglama, parametreler gibi ek yetenekler çekirdeğe dokunmadan
**overlay** olarak eklenir: her overlay yalnızca `RawNode`'un public `publish`/
`subscribe` API'sini kullanan bağımsız bir katmandır.

## Tasarım ilkeleri

Heap/virtual dispatch yok, template ile derleme-zamanı kaynak kontrolü,
donanımdan bağımsızlık, zero-copy LE ham byte — dört ilkenin tam anlatımı ve
kurulum/minimal örnek: repo kökündeki `README.md`.

## Mimari: çekirdek + overlay'ler

Çekirdek (`RawNode` ve altındaki framer/parser/broker) yalnızca opak datagramları
taşır. Üstüne oturan overlay'ler (reliability CH249, logging CH248, parameters
CH247/246) protokol yetenekleri ekler ama çekirdeği hiç değiştirmez. Rezerve
kanal bloğu ve overlay sözleşmesinin tam anlatımı: `overlays/README.md`.

## Nereden başlamalı

- \subpage minros-core "Çekirdek (core)" — framer, parser, broker, wire formatı.
- \subpage minros-interfaces "Interfaces" — mesaj tipleri (std_msgs, geometry_msgs).
- \subpage minros-overlays "Overlays" — reliability, logging, parameters.
- \subpage minros-utils "Utils" — delegate, endian, types yardımcıları.
- \subpage minros-maliyet "Donanım maliyeti" — RAM/wire/gecikme sayıları.

Tüm sınıf ve namespace referansı için üstteki arama kutusunu ya da soldaki gezinme
ağacını kullan.

## `minros` İsminin Hikayesi

`minros` adı bir GNU tarzı özyinelemeli kısaltmadır: **m**inros **i**s **n**ot **ros**.

Bu isim, bir projede alt sistemin (MCU) üst sistemle (PC) haberleşmesini çözerken
ortaya çıktı. Üst sistem ROS2 ile yazıldığından, diğer projelerde olduğu gibi
micro-ROS kullanılması planlanıyordu. Ama micro-ROS bu iş için gereğinden ağır
kaldı; istenen şey kanal tabanlı, hafif bir protokoldü. Zaten projedeki alt
sistem soyutlama katmanı sayesinde bu protokol ROS2'ye doğrudan bağlı
olmayacaktı — madem bir şekilde bir protokol yazılacaktı ve ortalıkta bu
sadelikte kanal tabanlı pub/sub çözümü yoktu, kendi protokolümüzü yazmaya
karar verildi.

Bunu anlatırken sürekli "kendi ROS'unuzu mu yapıyorsunuz?" tepkisiyle
karşılaşıldı. Bu karışıklığı çözmek yerine kucaklamak için ismin GNU'nun
özyinelemeli kısaltma geleneğine göz kırpması istendi; önce akla gelen `nros`
oldu. Sonra, micro-ROS yerine tercih edilmesi ve mikrodenetleyicide — yani
*minimum* kaynakla — çalışacak olması birleşince isim netleşti: **minros**.
