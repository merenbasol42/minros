#pragma once

#include <minros/utils/utils.hpp>


/// @file logging_protocol.hpp
/// @brief Logging overlay wire protokolü — kanal id'i, FLAGS bit yerleşimi.
///
/// Rezerve kanal bloğu (protokol overlay'leri): bkz.
/// [`minros/overlays/README.md`](../README.md). Tam gerekçe ve örnek akış:
/// logging-protocol.md.
///
/// Log frame'i (CH_ID = @ref minros::overlays::logging::protocol::LOG_CHANNEL_ID "LOG_CHANNEL_ID", payload):
///   - `[FLAGS(1)][text/bytes...]`
///   - FLAGS opak bir head önekidir; core anlamını bilmez (reliability seq gibi).

namespace minros::overlays::logging::protocol {

    constexpr u8 LOG_CHANNEL_ID = 248;  ///< düğüm → host (best-effort log yayını).

    /// @brief Log seviyesi.
    enum class Level : u8 {
        DEBUG = 0,
        INFO  = 1,
        WARN  = 2,
        ERROR = 3,
        FATAL = 4
    };

    /// @brief FLAGS bit yerleşimi.
    ///
    /// ```
    /// bit 0       LAST   : 1 → bu, log'un son (veya tek) parçası
    /// bit 1..3    LEVEL  : seviye (0..4), her parçada taşınır → parse tekdüze
    /// bit 4..7    SEQ4   : 0..15 dönen parça sayacı; kayıp/atlama tespiti
    /// ```
    ///
    /// Tek frame'e sığan log: LAST=1, SEQ4=0. Parçalı log: her parça SEQ4'ü +1
    /// taşır; sink süreklilik kontrolü yapar. Kanal unreliable (best-effort):
    /// parça düşerse sink o log'u atar, bozuk satır üretmez.

    /// @brief FLAGS baytını paketler.
    /// @param level Log seviyesi.
    /// @param seq4  0..15 dönen parça sayacı.
    /// @param last  Bu, log'un son (veya tek) parçasıysa `true`.
    /// @return Paketlenmiş FLAGS baytı.
    constexpr u8 pack_flags(Level level, u8 seq4, bool last) {
        return static_cast<u8>(
            (static_cast<u8>(seq4 & 0x0F) << 4) |
            ((static_cast<u8>(level) & 0x07) << 1) |
            (last ? 0x01u : 0x00u)
        );
    }

    /// @brief FLAGS'tan LAST bitini ayıklar.
    constexpr bool  flag_last (u8 flags) { return (flags & 0x01) != 0; }
    /// @brief FLAGS'tan LEVEL alanını ayıklar.
    constexpr Level flag_level(u8 flags) { return static_cast<Level>((flags >> 1) & 0x07); }
    /// @brief FLAGS'tan SEQ4 alanını ayıklar.
    constexpr u8    flag_seq  (u8 flags) { return static_cast<u8>((flags >> 4) & 0x0F); }

} // namespace minros::overlays::logging::protocol
