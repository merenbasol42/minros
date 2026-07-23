#pragma once

#include <minros/utils/utils.hpp>


namespace minros::overlays::parameters::protocol {

    /// @file parameters_protocol.hpp
    /// @brief Parameters overlay wire protokolü — kanal id'leri, op-code'lar, hata kodları.
    ///
    /// İki kanal (REQ/RES), reliability'nin "kanal başına tek publisher"
    /// sözleşmesini sağlar. Değer wire'da tipsiz, sabit-boyutlu little-endian
    /// byte bloğudur; tip host manifest'inde yaşar. Tam gerekçe ve örnek akış:
    /// parameters-protocol.md.
    ///
    /// Frame'ler (payload):
    ///   - GET   : `[OP=0x01][PARAM_ID]`
    ///   - SET   : `[OP=0x02][PARAM_ID][value bytes...]`
    ///   - VALUE : `[OP=0x03][PARAM_ID][value bytes...]`
    ///   - ERR   : `[OP=0x04][PARAM_ID][CODE]`

    constexpr u8 PARAM_REQ_CHANNEL_ID = 247;  ///< host → düğüm (GET, SET).
    constexpr u8 PARAM_RES_CHANNEL_ID = 246;  ///< düğüm → host (VALUE, ERR).

    /// @brief Frame payload'ının ilk baytı — işlem türü.
    enum class OpCode : u8 {
        GET   = 0x01,   ///< PARAM_REQ: parametrenin güncel değerini ister.
        SET   = 0x02,   ///< PARAM_REQ: değeri yazar (LE byte'lar).
        VALUE = 0x03,   ///< PARAM_RES: GET yanıtı ve/veya SET onayı.
        ERR   = 0x04,   ///< PARAM_RES: işlem reddedildi (@ref ErrCode).
    };

    /// @brief ERR frame'inin CODE baytı.
    enum class ErrCode : u8 {
        UNKNOWN_ID = 0x00,   ///< Bu ID'de kayıtlı parametre yok.
        READ_ONLY  = 0x01,   ///< Parametre salt-okunur, yazılamaz.
        BAD_LENGTH = 0x02,   ///< Value bytes uzunluğu tipin SIZE'ından kısa.
        REJECTED   = 0x03,   ///< Event handler (BEFORE_SET) değişikliği reddetti.
    };

} // namespace minros::overlays::parameters::protocol
