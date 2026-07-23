#pragma once
#include "minros/utils/utils.hpp"

/// @file broker.hpp
/// @brief Broker — gelen frame DATA alanını ayrıştırıp CH_ID bazında
///        callback'lere dağıtan çekirdek bileşen.

namespace minros::core {

    /// @brief Frame DATA alanını CH_ID bazında kayıtlı callback'lere dağıtır.
    ///
    /// Frame DATA düzeni:
    /// ```
    /// CH_ID  : 1 byte       (arr[from+0])                     -> kanal kimliği
    /// PAYLOAD: size-1 bytes (arr[from+1] .. arr[from+size-1]) -> asıl veri
    /// ```
    ///
    /// @note core SEQ bilmez. Reliability seq'i payload'ın ilk baytında taşır ve
    ///       kendi subscriber wrapper'ında ayıklar — broker bunu opak veri görür.
    ///
    /// @tparam MAX_SUBS Maksimum eş zamanlı abonelik sayısı (varsayılan 16).
    ///                   Küçük MCU'larda 4-8 arası yeterli olabilir; her slot
    ///                   ARM32'de 12 byte RAM tutar.
    ///
    /// @code
    /// Broker<8> broker;
    /// broker.subscribe(ch_id, &MyClass::static_handler, obj);
    /// parser_.set_on_frame_completed(&Broker<8>::frame_cb, &broker);
    /// @endcode
    template<u8 MAX_SUBS = 16>
    class Broker {
    public:
        /// @brief Kanal callback imzası: `fn(payload, payload_len, ctx)`.
        using ChannelCallback = utils::delegate<void, u8*, u8>;

        /// @brief CH_ID'ye callback kaydeder.
        /// @param ch_id Callback'in dinleyeceği kanal kimliği.
        /// @param cb    Frame geldiğinde çağrılacak delegate.
        /// @return Slot doluysa (`MAX_SUBS` aşıldıysa) `false`.
        bool subscribe(u8 ch_id, ChannelCallback cb) {
            if (sub_count >= MAX_SUBS) return false;
            subs[sub_count++] = { ch_id, cb };
            return true;
        }

        /// @brief Parser'ın `on_frame_completed` callback'i olarak kullanılacak instance metodu.
        /// @param arr  Frame'in okunduğu buffer.
        /// @param from DATA alanının buffer içindeki başlangıç offseti.
        /// @param size DATA alanının uzunluğu (CH_ID dahil).
        void on_frame_completed(u8* arr, u8 from, u8 size) {

            u8  ch_id    = arr[from];       // DATA[0] = CH_ID
            u8* payload  = arr + from + 1;  // DATA[1..] = kullanıcı verisi
            u8  pay_len  = size - 1u;       // CH_ID çıkarıldı

            for (u8 i = 0; i < sub_count; i++) {
                if (subs[i].ch_id == ch_id && subs[i].cb.is_valid()) {
                    subs[i].cb(payload, pay_len);
                }
            }
        }

        /// @brief `Parser::set_on_frame_completed()` için statik köprü fonksiyonu.
        ///
        /// @code
        /// parser_.set_on_frame_completed(&Broker<N>::frame_cb, &broker);
        /// @endcode
        ///
        /// @param arr  Frame'in okunduğu buffer.
        /// @param from DATA alanının buffer içindeki başlangıç offseti.
        /// @param size DATA alanının uzunluğu (CH_ID dahil).
        /// @param obj  `Broker*` olarak yorumlanacak instance işaretçisi.
        static void frame_cb(u8* arr, u8 from, u8 size, void* obj) {
            static_cast<Broker*>(obj)->on_frame_completed(arr, from, size);
        }

    private:
        struct Subscription {
            u8      ch_id;
            ChannelCallback cb;
        };

        Subscription subs[MAX_SUBS]{};
        u8      sub_count = 0;
    };

} // namespace minros::core
