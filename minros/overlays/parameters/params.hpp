#pragma once

#include <cstring>
#include <type_traits>
#include <minros/utils/utils.hpp>
#include <minros/overlays/parameters/parameters_protocol.hpp>

/// @file params.hpp
/// @brief Parameters overlay — düğüm tarafı (registry + REQ/RES işleyici).
///
/// Düğümün parametrelerini host'a okunur/yazılır kılar. Bir kanal katmanı
/// (`ChannelT`: RawNode ya da reliability::Reliable) üzerinden çalışır;
/// PARAM_REQ'e abone olur, PARAM_RES'ten yanıt yollar. Core'a hiçbir şey eklemez.
///
/// Kayıtlar derleme-zamanı bir `constexpr` tablodur (bkz. @ref minros::overlays::parameters::table),
/// `.rodata`'ya yerleşir → registry RAM tüketmez. Değer wire'da tipsiz, ham
/// little-endian byte bloğudur → serileştirme yok, zero-copy/`memcpy`.
///
/// Protokolün tam gerekçesi, sınırları ve örnek akış: parameters-protocol.md.
/// @see minros::overlays::parameters::protocol

namespace minros {
namespace overlays {
namespace parameters {

/// @brief Tek parametre kaydı — derleme-zamanı literal, flash'a (`.rodata`) yerleşir.
///
/// Tip etiketi ya da serileştirme pointer'ı tutmaz; 32-bit ABI'de 8 byte.
struct Entry {
    u8    id      = 0;
    u8    size    = 0;         ///< Tipin sabit SIZE'ı (== sizeof, padding yok).
    u8    flags   = 0;         ///< bit0: read-only (@ref FLAG_READ_ONLY).
    void* storage = nullptr;   ///< Gerçek değişken/mesaj (kopya yok, in-place).
};

constexpr u8 FLAG_READ_ONLY = 0x01;  ///< @ref Entry::flags bit0 — parametre salt-okunur.

/// @brief SET akışında event handler'a verilen faz.
enum class Event : u8 {
    BEFORE_SET = 0,   ///< Önerilen değer; handler `false` dönerse reddedilir (yazılmaz).
    AFTER_SET  = 1,   ///< Değer storage'a yazıldı; bildirim (dönüş yok sayılır).
};

// ── Tablo builder yardımcıları ────────────────────────────────────────────────
// rw<ID>(&var) / ro<ID>(&var): tip MsgT pointer'dan çıkarılır, ham-byte yolunun
// ön koşulları burada derleme-zamanı doğrulanır, tabloya yalnızca id/size/flags/
// pointer yazılır. MsgT bundan sonra "buharlaşır".

namespace detail {
template<typename MsgT>
constexpr void assert_raw_ok() {
    static_assert(minros::utils::endian::NATIVE_IS_LITTLE,
                  "parameters ham-byte yolu yalnızca little-endian hedeflerde geçerli");
    static_assert(std::is_trivially_copyable_v<MsgT>,
                  "param tipi trivially-copyable olmalı (memcpy semantiği)");
    static_assert(sizeof(MsgT) == static_cast<unsigned>(MsgT::SIZE),
                  "param tipinde padding var; wire byte'ları bellek görüntüsüyle eşleşmez");
}
} // namespace detail

/// @brief Okunur/yazılır parametre girişi kurar (compile-time).
/// @tparam ID   Wire'daki parametre kimliği (0..255).
/// @tparam MsgT Depolama tipi; ham-byte ön koşulları burada static_assert'lenir.
/// @param storage Statik-ömürlü depolamanın adresi (global/static olmalı).
template<u8 ID, typename MsgT>
constexpr Entry rw(MsgT* storage) {
    detail::assert_raw_ok<MsgT>();
    return Entry{ ID, MsgT::SIZE, 0, static_cast<void*>(storage) };
}

/// @brief Salt-okunur parametre girişi kurar; SET denenirse READ_ONLY döner.
/// @copydetails rw
template<u8 ID, typename MsgT>
constexpr Entry ro(MsgT* storage) {
    detail::assert_raw_ok<MsgT>();
    return Entry{ ID, MsgT::SIZE, FLAG_READ_ONLY, static_cast<void*>(storage) };
}

/// @brief Sabit boyutlu tablo sarmalayıcısı; N giriş sayısından türer.
template<u8 N>
struct ParamTable {
    static_assert(N > 0, "parametre tablosu en az bir giriş içermeli");
    Entry data[N];
    u8    count;
};

/// @brief @ref rw / @ref ro girişlerinden derleme-zamanı parametre tablosu kurar.
/// @return `.rodata`'ya yerleşen `ParamTable<sizeof...(Es)>`.
template<typename... Es>
constexpr auto table(Es... es) {
    static_assert((std::is_same_v<Es, Entry> && ...),
                  "table() yalnızca rw<>/ro<> girişleri kabul eder");
    return ParamTable<static_cast<u8>(sizeof...(Es))>{
        { es... }, static_cast<u8>(sizeof...(Es)) };
}


/// @brief Parameters overlay — PARAM_REQ'e abone olur, PARAM_RES'ten yanıt yollar.
/// @tparam ChannelT subscribe/publish sağlayan kanal katmanı (RawNode ya da Reliable).
template<class ChannelT>
class Params {
public:
    /// @brief Doğrulama + değişim-bildirimi callback'i (overlay başına tek).
    ///
    /// İmza `bool(u8 id, Event ev, const u8* bytes, u8 len)`; `bytes` LE ham
    /// değerdir. Değeri okumak için `reinterpret_cast` DEĞİL, `MsgT::from_bytes`
    /// kullan: `bytes` frame buffer'ına işaret eder, hizalı olma garantisi yoktur
    /// (Cortex-M0'da unaligned erişim fault verir); from_bytes memcpy tabanlı →
    /// hem hizalama hem endian güvenli. @ref Event::BEFORE_SET fazında `false`
    /// dönmek değişikliği reddeder.
    using EventHandler = delegate<bool, u8, Event, const u8*, u8>;

    /// @brief Boş kurar (tablo sonradan @ref bind_table ile), PARAM_REQ'e abone olur.
    explicit Params(ChannelT& ch, EventHandler on_event = {})
        : ch_(&ch), on_event_(on_event) {
        ch_->subscribe(protocol::PARAM_REQ_CHANNEL_ID, {&Params::req_thunk, this});
    }

    /// @brief Span ctor — tablo `(const Entry*, n)` olarak verilir.
    Params(ChannelT& ch, const Entry* table, u8 n, EventHandler on_event = {})
        : ch_(&ch), table_(table), count_(n), on_event_(on_event) {
        ch_->subscribe(protocol::PARAM_REQ_CHANNEL_ID, {&Params::req_thunk, this});
    }

    /// @brief Kolaylık ctor — CTAD ile `Params{node, TABLE}` (ParamTable→span).
    template<u8 N>
    Params(ChannelT& ch, const ParamTable<N>& t, EventHandler on_event = {})
        : Params(ch, t.data, t.count, on_event) {}

    // this pointer'ı kanala kayıtlı → kopyalanamaz.
    Params(const Params&)            = delete;
    Params& operator=(const Params&) = delete;

    /// @brief Boş ctor ile kurulduysa flash constexpr tabloyu sonradan bağlar.
    void bind_table(const Entry* table, u8 n) { table_ = table; count_ = n; }
    /// @overload
    template<u8 N>
    void bind_table(const ParamTable<N>& t)   { table_ = t.data; count_ = t.count; }

    /// @brief Doğrulama/bildirim callback'ini ayarlar. @see EventHandler
    void set_event_handler(EventHandler h) { on_event_ = h; }

private:
    const Entry* find(u8 id) const {
        for (u8 i = 0; i < count_; i++)
            if (table_[i].id == id) return &table_[i];
        return nullptr;
    }

    // VALUE: [OP][ID] head + storage payload'ı (zero-copy, logger deseni).
    void send_value(const Entry& e) {
        head_[0] = static_cast<u8>(protocol::OpCode::VALUE);
        head_[1] = e.id;
        ch_->publish(protocol::PARAM_RES_CHANNEL_ID, head_, 2,
                     static_cast<const u8*>(e.storage), e.size);
    }

    void send_err(u8 id, protocol::ErrCode code) {
        head_[0] = static_cast<u8>(protocol::OpCode::ERR);
        head_[1] = id;
        head_[2] = static_cast<u8>(code);
        ch_->publish(protocol::PARAM_RES_CHANNEL_ID, head_, 3);
    }

    void handle(const u8* p, u8 len) {
        if (len < 2) return;
        const auto   op = static_cast<protocol::OpCode>(p[0]);
        const u8     id = p[1];
        const Entry* e  = find(id);

        switch (op) {
        case protocol::OpCode::GET:
            if (!e) { send_err(id, protocol::ErrCode::UNKNOWN_ID); return; }
            send_value(*e);
            return;

        case protocol::OpCode::SET: {
            if (!e)                       { send_err(id, protocol::ErrCode::UNKNOWN_ID); return; }
            if (e->flags & FLAG_READ_ONLY){ send_err(id, protocol::ErrCode::READ_ONLY);  return; }
            const u8 vlen = static_cast<u8>(len - 2u);   // [OP][ID] sonrası
            if (vlen < e->size)           { send_err(id, protocol::ErrCode::BAD_LENGTH); return; }

            const u8* val = p + 2;
            if (on_event_.is_valid() &&
                !on_event_(id, Event::BEFORE_SET, val, e->size)) {
                send_err(id, protocol::ErrCode::REJECTED); return;
            }

            // Zero-copy commit: gelen wire byte'ları LE hedefte storage görüntüsü.
            memcpy(e->storage, val, e->size);

            if (on_event_.is_valid())
                on_event_(id, Event::AFTER_SET,
                          static_cast<const u8*>(e->storage), e->size);

            send_value(*e);   // onay
            return;
        }

        default:
            return;   // VALUE/ERR/bilinmeyen: düğümde yoksay
        }
    }

    // Kanal ChannelCallback imzası: fn(payload, len, ctx).
    static void req_thunk(u8* payload, u8 len, void* ctx) {
        static_cast<Params*>(ctx)->handle(payload, len);
    }

    ChannelT*    ch_       = nullptr;
    const Entry* table_    = nullptr;   // flash'taki constexpr tabloya span
    u8           count_    = 0;
    EventHandler on_event_{};
    u8           head_[3]{};            // giden VALUE/ERR öneki (reliability için kalıcı)
};

} // namespace parameters
} // namespace overlays
} // namespace minros
