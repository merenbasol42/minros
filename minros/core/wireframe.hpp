#pragma once

#include "minros/utils/utils.hpp"

/// @file wireframe.hpp
/// @brief minros wire çerçevesi — frame düzeni, sabitler, CRC-8/SMBUS.
///
/// @ref minros::core::Framer ve @ref minros::core::Parser'ın paylaştığı tek
/// gerçek: HEADER/LEN/DATA/CRC alan sınırları ve checksum algoritması.

namespace minros::core::wireframe {

    constexpr u8 VERSION[4] = {0x00, 0x00, 0x01, 0x00};  ///< Kütüphane sürümü: major.minor.patch.build.

    /// @brief Wire frame düzeni.
    ///
    /// ```
    /// HEADER       : {0x6D, 0x72, 0x6F, 0x73}  ('m','r','o','s') — senkronizasyon
    /// LEN          : DATA uzunluğu (2..249)
    /// DATA         : DATA (LEN kadar)
    ///     CH_ID    : DATA[0] — kanal kimliği
    ///     PAYLOAD  : DATA[1..LEN-1] — kullanıcı verisi (1..248 byte)
    /// CRC          : CRC-8/SMBUS (poly=0x07, init=0x00) — DATA byte'ları üzerinden
    /// ```
    ///
    /// @note core katmanı SEQ bilmez. Güvenilirlik (reliability) katmanı kendi
    ///       sıra numarasını PAYLOAD'ın önüne opak bir baytlık önek olarak koyar.
    /// @note Endianness: wire formatı little-endian. std_msgs katmanı dönüşümü
    ///       @ref minros::utils::endian::store_le / load_le ile otomatik yönetir.

    constexpr u8 HEADER_SIZE   = 4u;                              ///< HEADER alanının bayt uzunluğu.
    constexpr u8 HEADER[HEADER_SIZE] = {0x6D, 0x72, 0x6F, 0x73};  ///< Senkronizasyon deseni ("mros").

    // DATA alanı sınırları. BUFFER_SIZE = HEADER(4) + LEN(1) + DATA(249) + CRC(1) = 255 (u8 max).
    constexpr u8 MAX_DATA_LEN  = 249u;              ///< DATA alanının maksimum uzunluğu.
    constexpr u8 MAX_PAYLOAD   = MAX_DATA_LEN - 1;  ///< Maksimum PAYLOAD uzunluğu (CH_ID çıkarıldı).
    constexpr u8 MIN_PAYLOAD   = 1u;                ///< Minimum PAYLOAD uzunluğu.
    constexpr u8 MIN_DATA_LEN  = 1u + MIN_PAYLOAD;  ///< Minimum DATA uzunluğu (CH_ID + MIN_PAYLOAD).

    /// @brief CRC-8/SMBUS güncelleme adımı (poly=0x07, init=0x00, xor-out=0x00).
    ///
    /// @ref minros::core::Parser ve @ref minros::core::Framer tarafından
    /// paylaşılır; tablo gerektirmez.
    ///
    /// @param crc  Önceki CRC durumu (ilk çağrıda 0x00).
    /// @param byte İşlenecek bir sonraki bayt.
    /// @return Güncellenmiş CRC durumu.
    inline constexpr u8 crc8_update(u8 crc, u8 byte) noexcept {
        crc ^= byte;
        for (u8 i = 0; i < 8u; i++) {
            crc = (crc & 0x80u)
                ? static_cast<u8>((crc << 1u) ^ 0x07u)
                : static_cast<u8>(crc << 1u);
        }
        return crc;
    }

} // namespace minros::core::wireframe
