#pragma once
#include "minros/core/parser.hpp"
#include "minros/core/broker.hpp"
#include "minros/core/framer.hpp"
#include "minros/utils/utils.hpp"


/// @file raw_node.hpp
/// @brief RawNode — saf ham byte datagram API (kanal bazlı publish/subscribe).

namespace minros {

    /// @brief Kullanıcı tarafından verilen dört IO callback'i — donanımdan bağımsızlığı sağlar.
    struct Transport {
        utils::delegate<void, u8*, u8> send_bytes;  ///< `fn(buf, len)` — frame'i fiziksel katmana yazar.
        utils::delegate<void, u8*, u8> read_bytes;  ///< `fn(buf, max_len)` — fiziksel katmandan okur.
        utils::delegate<u8>            get_size;    ///< `fn()` — okunmayı bekleyen bayt sayısını döner.
        utils::delegate<u32>           get_time;    ///< `fn()` — milisaniye cinsinden monotonik zaman döner.
    };


    /// @brief Saf ham byte datagram API (kanal bazlı publish/subscribe).
    ///
    /// Reliability'den habersizdir. Güvenilirlik isteyen, bu RawNode'un public
    /// API'sini kullanan @ref minros::overlays::reliability::Reliable overlay'ini
    /// takar.
    ///
    /// @tparam MAX_SUBS       Maksimum abonelik sayısı (varsayılan 16).
    ///                        Reliable kullanılıyorsa ACK kanalı için +1 hesaba katılmalı.
    /// @tparam MAX_FRAME_DATA Frame DATA alanı maksimum uzunluğu (varsayılan 249).
    ///
    /// @code
    /// RawNode<> node;
    /// node.transport = { ... };
    ///
    /// node.subscribe(ch_id, {on_bytes, ctx});   // fn(payload, len, ctx)
    /// node.publish(ch_id, buf, len);
    ///
    /// node.spin_once();   // loop() içinde
    /// @endcode
    template<u8 MAX_SUBS       = 16,
             u8 MAX_FRAME_DATA = core::wireframe::MAX_DATA_LEN>
    class RawNode {
        static_assert(MAX_FRAME_DATA >= core::wireframe::MIN_DATA_LEN &&
                      MAX_FRAME_DATA <= core::wireframe::MAX_DATA_LEN,
                      "MAX_FRAME_DATA aralık dışı");
        static_assert(MAX_SUBS > 0, "MAX_SUBS en az 1 olmalı");

        using BrokerT = core::Broker<MAX_SUBS>;

    public:
        /// @brief Kanal callback imzası: `fn(payload, len, ctx)`.
        using ChannelCallback = typename BrokerT::ChannelCallback;

        RawNode() {
            parser_.set_on_frame_completed({&BrokerT::frame_cb, &broker_});
        }

        /// @brief Transport'tan bekleyen baytları okuyup ayrıştırır. Ana döngüde çağrılır.
        void spin_once() { feed_parser(); }

        /// @brief Ham payload gönderir.
        /// @param ch_id   Kanal kimliği.
        /// @param payload Kullanıcı verisi.
        /// @param len     `payload` uzunluğu.
        /// @return `transport.send_bytes` tanımlı değilse veya frame `Framer` sınırını aşarsa `false`.
        bool publish(u8 ch_id, const u8* payload, u8 len) {
            return raw_publish(ch_id, nullptr, 0, payload, len);
        }

        /// @brief Opak HEAD öneki + payload gönderir (katmanlı protokoller, örn. reliability seq).
        ///
        /// Core head'in anlamını bilmez; tele CH_ID + head + payload olarak girer.
        ///
        /// @param ch_id     Kanal kimliği.
        /// @param head      Opak head öneki, `nullptr` olabilir.
        /// @param head_len  `head` uzunluğu (`head == nullptr` ise 0 olmalı).
        /// @param payload   Kullanıcı verisi.
        /// @param len       `payload` uzunluğu.
        /// @return `transport.send_bytes` tanımlı değilse veya frame `Framer` sınırını aşarsa `false`.
        bool publish(u8 ch_id, const u8* head, u8 head_len, const u8* payload, u8 len) {
            return raw_publish(ch_id, head, head_len, payload, len);
        }

        /// @brief CH_ID'ye callback kaydeder.
        /// @param ch_id Kanal kimliği.
        /// @param cb    Frame geldiğinde çağrılacak delegate.
        /// @return Broker'ın abonelik slotu doluysa `false`.
        bool subscribe(u8 ch_id, ChannelCallback cb) {
            return broker_.subscribe(ch_id, cb);
        }

        /// @brief `node.transport = { ... };` ile doğrudan atama yapılan IO callback demeti.
        Transport transport;

    private:
        bool raw_publish(u8 ch_id, const u8* head, u8 head_len,
                         const u8* payload, u8 len) {
            if (!transport.send_bytes.is_valid())                       return false;
            if (!framer_.build(ch_id, head, head_len, payload, len))    return false;
            transport.send_bytes(framer_.data(), framer_.size());
            return true;
        }

        void feed_parser() {
            auto w = parser_.write_window();
            u8 n = transport.get_size();
            if (n > w.size) n = w.size;
            transport.read_bytes(w.data, n);
            parser_.commit(n);
        }

        core::Parser<MAX_FRAME_DATA> parser_;
        BrokerT                      broker_;
        core::Framer<MAX_FRAME_DATA> framer_;
    };

} // namespace minros
