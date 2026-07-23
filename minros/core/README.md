# Core (Çekirdek) {#minros-core}

Çekirdek (`core`), yalnızca opak datagramları taşıyan alt katmandır: wire
formatı, framing, parsing ve kanal bazlı dağıtım. Hiçbir overlay veya mesaj
tipini bilmez.

| Dosya | Ne yapar |
|---|---|
| [`wireframe.hpp`](wireframe.hpp) | Frame düzeni, sabitler, CRC-8/SMBUS |
| [`framer.hpp`](framer.hpp) | Ham payload'ı wire formatına serileştirir |
| [`parser.hpp`](parser.hpp) | Gelen byte akışını durum makinesiyle ayrıştırır |
| [`broker.hpp`](broker.hpp) | Ayrıştırılan DATA'yı CH_ID bazında callback'lere dağıtır |

Mimari anlatımı ve overlay'lerle ilişkisi için bkz. ana sayfadaki
"Mimari: çekirdek + overlay'ler" bölümü; ayrı bir tasarım dokümanı gerektirmez
(bkz. `docs/DOCUMENTATION.md` Kural 4).
