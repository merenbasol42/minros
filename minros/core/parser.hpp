#pragma once

#include <minros/utils/utils.hpp>
#include <minros/core/wireframe.hpp>


/// @file parser.hpp
/// @brief Parser — gelen wire byte akışını durum makinesiyle ayrıştırıp tamamlanan frame'leri bildiren çekirdek bileşen.

namespace minros::core {

    /// @brief Wire byte akışını @ref wireframe düzenine göre ayrıştırır.
    ///
    /// Checksum: CRC-8/SMBUS (polinom 0x07, init 0x00).
    /// @ref wireframe::crc8_update paylaşılan implementasyon — @ref Framer ile tutarlı.
    ///
    /// @tparam MAX_DATA DATA alanının maksimum uzunluğu (varsayılan
    ///                  @ref wireframe::MAX_DATA_LEN). Küçük tutulursa buffer
    ///                  belleği doğrudan küçülür.
    template<u8 MAX_DATA = wireframe::MAX_DATA_LEN>
    class Parser {
        static_assert(MAX_DATA >= wireframe::MIN_DATA_LEN &&
                      MAX_DATA <= wireframe::MAX_DATA_LEN,
                      "MAX_DATA: MIN_DATA_LEN..wireframe::MAX_DATA_LEN aralığında olmalı");

        static constexpr u8 BUFFER_SIZE =
            wireframe::HEADER_SIZE +
            1u /* length field */  +
            MAX_DATA               +
            1u /* checksum field */;

    public:
        /// @brief Parser hata kodları.
        enum class Error : u8 {
            INVALID_LENGTH,  ///< length alanı MIN_DATA_LEN'den küçük veya MAX_DATA sınırını aşıyor.
            CRC_MISMATCH,    ///< alınan CRC hesaplanan CRC ile eşleşmiyor.
        };

        /// @brief Tamamlanan frame callback imzası: `fn(buffer, data_start, data_len, ctx)`.
        using FrameCallback = utils::delegate<void, u8*, u8, u8>;
        /// @brief Ayrıştırma hatası callback imzası: `fn(Error, ctx)`.
        using ErrorCallback = utils::delegate<void, Error>;

        /// @brief `write_window()`'ın döndürdüğü, doğrudan yazılabilir bölge.
        struct Slice { u8* data; u8 size; };

        Parser() { reset(); }

        /// @brief Doğrudan yazılabilecek bölgeyi döner — zero-copy okuma için.
        Slice write_window() {
            return { buffer + write_pos, static_cast<u8>(BUFFER_SIZE - write_pos) };
        }

        /// @brief `write_window()`'a n bayt yazıldıktan sonra çağrılır; parse eder.
        /// @param n `write_window()`'dan alınan bölgeye yazılan bayt sayısı.
        void commit(u8 n) {
            write_pos += n;
            while (parse_pos < write_pos) {
                advance();
            }
            // frame_start'tan önceki her şey (ne HEADER_WAIT'teki eşleşmeyen
            // gürültü, ne de -bulunmuş bir header'dan sonra- LENGTH/DATA/CRC
            // bekleyen bir frame'in önündeki çöp önek) bir daha asla okunmaz.
            // compact() bunu her commit() sonunda geri kazanır — aksi halde
            // hem sürekli gürültüde write_window() kalıcı olarak sıfıra düşer
            // hem de büyük bir frame'in DATA'sı, önündeki çöp yüzünden
            // buffer'a asla tam sığmayabilir.
            compact();
        }

        /// @brief Bir frame tamamlandığında çağrılacak callback'i ayarlar.
        void set_on_frame_completed(FrameCallback cb) { on_frame_completed = cb; }
        /// @brief Ayrıştırma hatası oluştuğunda çağrılacak callback'i ayarlar.
        void set_on_error(ErrorCallback cb)           { on_error = cb; }

    private:
        enum class State : u8 {
            HEADER_WAIT,
            LENGTH_WAIT,
            DATA_READING,
            CRC_WAIT
        };

        void advance() {
            u8 byte = buffer[parse_pos];
            switch (state) {

                case State::HEADER_WAIT:
                    if (byte == wireframe::HEADER[header_matched]) {
                        parse_pos++;
                        header_matched++;
                        if (header_matched == wireframe::HEADER_SIZE) {
                            state = State::LENGTH_WAIT;
                        }
                    } else {
                        // Sadece bu baytı tüket; buffer'daki geri kalanlar geçerli.
                        parse_pos++;
                        // Hatalı bayt yeni bir header başlangıcı olabilir.
                        header_matched = (byte == wireframe::HEADER[0]) ? 1 : 0;
                    }
                    // Değişmez: frame_start, süregelen eşleşmenin (varsa) başlangıcı;
                    // header_matched==0 ise "buraya kadarki her şey kesin çöp" demek —
                    // compact()'in atabileceği sınırı bu satır tek elden belirler.
                    frame_start = parse_pos - header_matched;
                    break;

                case State::LENGTH_WAIT:
                    // DATA en az CH_ID(1) + MIN_PAYLOAD(1) = MIN_DATA_LEN byte olmalı
                    if (byte < wireframe::MIN_DATA_LEN || byte > MAX_DATA) {
                        on_error(Error::INVALID_LENGTH);
                        // length baytını tüketme: HEADER dizisiyle self-overlap
                        // yapmadığından yeni bir header tam bu baytta başlayabilir.
                        resync();
                        break;
                    }
                    parse_pos++;
                    data_len       = byte;
                    data_remaining = byte;
                    data_start     = parse_pos; // DATA, length byte'ından hemen sonra başlar
                    crc            = 0;
                    state          = State::DATA_READING;
                    break;

                case State::DATA_READING:
                    parse_pos++;
                    crc = wireframe::crc8_update(crc, byte);
                    data_remaining--;
                    if (data_remaining == 0) {
                        state = State::CRC_WAIT;
                    }
                    break;

                case State::CRC_WAIT:
                    if (byte == crc) {
                        parse_pos++;  // CRC baytını tüket
                        if (on_frame_completed.is_valid()) {
                            on_frame_completed(buffer, data_start, data_len);
                        }
                        finish_frame();
                    } else {
                        on_error(Error::CRC_MISMATCH);
                        // DATA/CRC olarak tüketilmiş baytları atmadan yeniden tara —
                        // içlerinde gerçek bir frame'in header'ı gömülü olabilir.
                        resync();
                    }
                    break;

                default:
                    discard_before(parse_pos);  // erişilemez dal — savunmacı sıfırlama
                    break;
            }
        }

        // Bir frame başarıyla tamamlanınca çağrılır.
        // parse_pos'tan write_pos'a kadar olan baytları buffer başına kaydırır;
        // bir sonraki frame için state'i sıfırlar.
        // Bir spin_once'ta birden fazla frame varsa veriler kaybolmaz.
        void finish_frame() {
            discard_before(parse_pos);
        }

        // Geçersiz LENGTH veya CRC_MISMATCH sonrası çağrılır.
        // Eşleşmiş header'ı (wireframe::HEADER kendi içinde çakışmadığından
        // "header değil" olduğu kesin ispatlanmış tek bölge) atar, ama DATA/CRC
        // olarak tüketilmiş baytları atmaz — gömülü olabilecek gerçek bir header
        // için HEADER_WAIT'in onları yeniden taramasına izin verir.
        void resync() {
            discard_before(static_cast<u8>(frame_start + wireframe::HEADER_SIZE));
        }

        // buffer[from..write_pos) aralığını buffer başına kaydırır, write_pos'u
        // günceller ve yeni write_pos'u döner. parse_pos ve diğer alanların nasıl
        // güncelleneceği çağırana bırakılmıştır — discard_before() ve compact()
        // bu konuda kasıtlı olarak farklı davranır (bkz. altlarındaki açıklamalar).
        u8 shift_buffer(u8 from) {
            u8 remaining = write_pos - from;
            for (u8 i = 0; i < remaining; i++) {
                buffer[i] = buffer[from + i];
            }
            write_pos = remaining;
            return remaining;
        }

        // parse_pos hariç tüm frame alanlarını sıfırlar; write_pos'u parametreyle
        // verilen değere set eder. reset() ve discard_before() ortak son adımıdır.
        void reset_frame_state(u8 new_write_pos) {
            write_pos      = new_write_pos;
            parse_pos      = 0;
            header_matched = 0;
            frame_start    = 0;
            data_start     = 0;
            data_len       = 0;
            data_remaining = 0;
            crc            = 0;
            state          = State::HEADER_WAIT;
        }

        // Bir frame başarıyla tamamlanınca (finish_frame()) ya da geçersiz
        // LENGTH/CRC_MISMATCH sonrası (resync()) çağrılır: buffer[from..write_pos)
        // aralığını başa kaydırır ve state'i tamamen sıfırlar. parse_pos bilinçli
        // olarak 0'a çekilir — amaç kaydırılan baytları (resync()'te olduğu gibi)
        // HEADER_WAIT'ten yeniden taratmak.
        void discard_before(u8 from) {
            u8 remaining = shift_buffer(from);
            reset_frame_state(remaining);
        }

        // Her commit() sonunda çağrılır. frame_start'tan önceki hiçbir bayt bir
        // daha asla okunmaz — ister HEADER_WAIT'te eşleşmeyen gürültü olsun,
        // ister bulunmuş bir header'ın (LENGTH_WAIT/DATA_READING/CRC_WAIT
        // sırasında) önünde kalmış eski çöp olsun. compact() bu öneki buffer
        // başına kaydırıp atar; discard_before()'ın aksine state'i sıfırlamaz,
        // header_matched/data_len/data_remaining/crc gibi "offset olmayan"
        // alanlara dokunmaz — sadece buffer içindeki konum bilgilerini
        // (write_pos, parse_pos, data_start) kaymaya göre günceller. Böylece:
        //   • sürekli gürültüde write_window() kalıcı olarak sıfıra düşmez,
        //   • buffer'ın ortasında bulunan bir header'dan sonra büyük bir DATA
        //     alınırken, önündeki çöp yüzünden yer sıkışmaz.
        void compact() {
            if (frame_start == 0) return;  // atılacak bir şey yok
            u8 shift = frame_start;
            shift_buffer(shift);
            parse_pos -= shift;
            if (state == State::DATA_READING || state == State::CRC_WAIT) {
                data_start -= shift;  // DATA'nın buffer içindeki yeri de kaymış oldu
            }
            frame_start = 0;
        }

        // Sadece constructor'dan çağrılır.
        void reset() {
            reset_frame_state(0);
        }

        FrameCallback on_frame_completed;
        ErrorCallback on_error;

        u8    buffer[BUFFER_SIZE];
        u8    write_pos;       // producer — yeni baytların yazıldığı konum
        u8    parse_pos;       // consumer — parser'ın okumakta olduğu konum
        u8    header_matched;  // eşleşen header bayt sayısı
        u8    frame_start;     // mevcut header denemesinin buffer içindeki başlangıç offseti
        u8    data_start;      // DATA bölümünün buffer içindeki başlangıç offseti
        u8    data_len;        // LENGTH alanından gelen toplam DATA uzunluğu
        u8    data_remaining;  // okunmayı bekleyen DATA bayt sayısı
        u8    crc;             // DATA baytları üzerindeki kayan CRC-8 değeri
        State state;
    };

} // namespace minros::core
