# Parameters {#minros-overlays-parameters}

İki kanal (REQ CH247 / RES CH246) — host GET/SET ister, düğüm VALUE/ERR
yanıtlar. Kayıtlar derleme-zamanı `constexpr` tablodur (`.rodata`'ya yerleşir,
RAM tüketmez); değer wire'da tipsiz, ham little-endian byte bloğudur (zero-copy
`memcpy`, serileştirme yok). Head öneki yoktur — REQ/RES için ayrı kanal
kullanımı, reliability'nin kanal-başına-tek-publisher sözleşmesini karşılar.

Wire kontratı, tasarım gerekçesi ve akış: \subpage minros-overlays-parameters-protocol
"parameters-protocol.md".

Header Doxygen: [`params.hpp`](params.hpp), [`parameters_protocol.hpp`](parameters_protocol.hpp).
