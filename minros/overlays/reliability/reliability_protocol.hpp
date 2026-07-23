#pragma once

#include <minros/utils/utils.hpp>


/// @file reliability_protocol.hpp
/// @brief Reliability overlay wire protokolü — ACK kanal id'i, response tipi.
///
/// Rezerve kanal bloğu (protokol overlay'leri): bkz.
/// [`minros/overlays/README.md`](../README.md). Tam gerekçe ve örnek akış:
/// reliability-protocol.md.
///
/// Reliable veri frame'i (CH_ID = kullanıcı kanalı):
///   - `PAYLOAD = [SEQ(1)][user bytes...]`
///   - SEQ, Reliable katmanının payload'a koyduğu opak önek; core bunu bilmez.
///
/// ACK frame'i (CH_ID = @ref minros::overlays::reliability::protocol::ACK_CHANNEL_ID "ACK_CHANNEL_ID", payload):
///   - `RESP`  : 1 byte = 0x06 (response tipi, ASCII ACK)
///   - `CH_ID` : 1 byte (ACK'lenen kanal)
///   - `SEQ`   : 1 byte (ACK'lenen sequence numarası)
///
/// @note RESP alanı şu an sadece ACK var, NACK yok; ileride eklenebilsin diye ayrıldı.

namespace minros::overlays::reliability::protocol {

    /// @brief ACK frame'inin RESP baytı.
    enum class ResponseType: u8 {
        ACK = 0x06
    };

    constexpr u8 ACK_CHANNEL_ID = 249;  ///< düğüm ↔ host (stop-and-wait ACK).

} // namespace minros::overlays::reliability::protocol
