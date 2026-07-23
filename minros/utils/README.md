# Utils {#minros-utils}

`utils`, çekirdek ve overlay'lerin ortak kullandığı bağımsız yardımcı
tipler/fonksiyonlardır. Heap ve virtual dispatch kullanmaz.

| Dosya | Ne yapar |
|---|---|
| [`types.hpp`](types.hpp) | Sabit-genişlikli tamsayı/float takma adları |
| [`endian.hpp`](endian.hpp) | `store_le` / `load_le` — little-endian dönüşüm |
| [`delegate.hpp`](delegate.hpp) | Tip-güvenli, heap'siz callback sarmalayıcısı |
| [`utils.hpp`](utils.hpp) | Şemsiye header — üçünü tek include'da toplar |

Ayrıntılı API için header Doxygen yorumlarına bakın; ayrı bir tasarım dokümanı
gerektirmez (bkz. `docs/DOCUMENTATION.md` Kural 4).
