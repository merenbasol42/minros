#pragma once
#include "minros/utils/utils.hpp"

/// @file msg_base.hpp
/// @brief MsgBase — mesaj tiplerinin türediği CRTP (Curiously Recurring
///        Template Pattern) temel sınıfı.

namespace minros::interfaces {

/// @brief Mesaj tiplerinin türediği CRTP temel sınıfı.
///
/// Virtual dispatch yerine CRTP kullanılır:
///
/// | | Virtual | CRTP |
/// |---|---|---|
/// | Bellek | Her object'ta +8 byte (vptr) | +0 byte |
/// | Çağrı | Indirect call (cache miss) | Direkt call (inlineable) |
/// | Çözümleme | Runtime'da çözümlenir | Derleme zamanında çözümlenir |
/// | Optimizasyon | Derleyici inline edemez | Derleyici tamamen optimize eder |
///
/// @tparam Derived Türeyen mesaj tipi (`struct Verim : MsgBase<Verim> { ... };`).
template<typename Derived>
struct MsgBase {

    /// @brief `buf`'tan `Derived`'ı deserileştirir.
    ///
    /// Ortak size check burada yapılır — her türeyen sınıfta tekrarlanmaz.
    ///
    /// @note serialize/deserialize, @ref minros::utils::endian::store_le /
    ///       load_le üzerinden çalışır; wire formatı little-endian'dır, host
    ///       dönüşümü otomatiktir.
    ///
    /// @param buf Kaynak byte dizisi.
    /// @param len `buf` uzunluğu.
    /// @return `len < Derived::SIZE` ise `false`.
    [[nodiscard]] bool from_bytes(const u8* buf, u8 len) noexcept {
        if (len < Derived::SIZE) return false;
        // static_cast: virtual dispatch değil, derleme zamanında bağlı
        static_cast<Derived*>(this)->deserialize(buf);
        return true;
    }

    /// @brief `Derived`'ı `buf`'a serileştirir. Çağıran, `buf`'ın en az
    ///        `Derived::SIZE` bayt olduğundan emin olmalıdır.
    void to_bytes(u8* buf) const noexcept {
        static_cast<const Derived*>(this)->serialize(buf);
    }

    /// @brief Mesaj boyutunu tip üzerinden sorgular.
    static constexpr u8 size() noexcept { return Derived::SIZE; }

    /// @brief Mesaj ailesi kimliği.
    ///
    /// Mesaj tipi kimliği iki parçalıdır: `[FAMILY_ID][TYPE_ID]`.
    /// - `family_id()` — mesaj ailesi (paket).
    /// - `type_id()`   — aile içindeki mesaj kimliği (aile-yerel).
    ///
    /// FAMILY_ID açık bir kayıt uzayıdır (kapalı enum değil). Numaralandırma
    /// aralık şemasıyla yönetilir; yeni aile eklemek çekirdek koda dokunmaz:
    /// - `0x00`–`0x7F` resmi / rezerve aileler — proje tahsis eder
    ///   (`std_msgs` = 0x00, `geometry_msgs` = 0x01, `sensor_msgs` = 0x02, ...).
    /// - `0x80`–`0xFF` özel kullanım (private) — herkes koordinasyonsuz kullanır;
    ///   resmi aileler bu bloğu ASLA almaz → çakışma garantili yok.
    static constexpr u8 family_id() noexcept { return Derived::FAMILY_ID; }
    /// @brief Aile içindeki mesaj kimliği (aile-yerel). @see family_id
    static constexpr u8 type_id()   noexcept { return Derived::TYPE_ID; }
};

/// @brief `msg`'i `buf`'a serileştirir. Yeni mesaj tipi eklendiğinde bu
///        fonksiyon değişmez — template otomatik genişler.
template<typename Derived>
void serialize_to(const MsgBase<Derived>& msg, u8* buf) noexcept {
    msg.to_bytes(buf);
}

/// @brief `buf`'tan `msg`'i deserileştirir. Yeni mesaj tipi eklendiğinde bu
///        fonksiyon değişmez — template otomatik genişler.
/// @return `len < Derived::SIZE` ise `false`.
template<typename Derived>
[[nodiscard]] bool deserialize_from(MsgBase<Derived>& msg,
                                    const u8* buf, u8 len) noexcept {
    return msg.from_bytes(buf, len);
}

}  // namespace minros::interfaces
