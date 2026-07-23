#pragma once
#include "minros/raw_node.hpp"
#include "minros/overlays/reliability/reliable.hpp"
#include "minros/overlays/logging/logger.hpp"
#include "minros/overlays/parameters/params.hpp"

#include <cstring>


/// @file node.hpp
/// @brief Node — yüksek seviye tipli API (RawNode + reliability::Reliable sarmalayıcı).

namespace minros {

    /// @brief Yüksek seviye tipli düğüm (RawNode + reliability sarmalayıcı + logging + parameters).
    ///
    /// Parametreler artık derleme-zamanı bir constexpr tablo ile tanımlanır
    /// (bkz. @ref set_params); bu yüzden ayrı bir `MAX_PARAMS` template'i yoktur.
    ///
    /// @tparam MAX_SUBS       Maksimum abonelik sayısı (varsayılan 16).
    ///                        Dahili broker'a ACK + PARAM_REQ için +2 slot verilir.
    /// @tparam MAX_FRAME_DATA Frame DATA alanı maksimum uzunluğu (varsayılan 249).
    /// @tparam MAX_RELIABLE   Güvenilir publisher/subscriber başına maksimum kanal (varsayılan 8).
    ///
    /// @code
    /// minros::Node<> node;
    /// node.transport = { ... };
    ///
    /// // Publisher
    /// auto pub = node.create_publisher<Twist>(ch_id);
    /// pub.publish(msg);
    ///
    /// // Subscriber — callback doğrudan tipli mesaj alır
    /// node.create_subscription<Twist>(ch_id, {on_cmd, ctx});
    /// // fn imzası: void on_cmd(const Twist& msg, void* ctx)
    ///
    /// // Güvenilir publisher — buffer'ı Publisher kendi içinde tutar
    /// auto pub = node.create_publisher<Twist>(ch_id, /*reliable=*/true);
    /// if (!pub.publish(msg)) { /* önceki hâlâ uçuşta, sonra dene */ }
    ///
    /// // Güvenilir subscriber
    /// node.create_subscription<Twist>(ch_id, {on_cmd, ctx}, /*reliable=*/true);
    ///
    /// // Logging (best-effort, CH248) — yalnızca yayın, zero-buffer
    /// node.set_log_level(Node<>::LogLevel::INFO);   // DEBUG bastırılır
    /// node.log_info("motor started");
    /// node.log_error("imu timeout");
    /// // Log ALMAK için host tarafında logging::LogSink kullanılır (minrospy).
    ///
    /// // Parameters (best-effort, CH247 REQ / CH246 RES) — host get/set eder.
    /// // Tablo constexpr'dır → flash'ta yaşar; storage'lar statik ömürlü olmalı.
    /// static std_msgs::Float32 kp;
    /// static geometry_msgs::Vector3 gain;
    /// constexpr auto TABLE = overlays::parameters::table(
    ///     overlays::parameters::rw<5>(&kp),
    ///     overlays::parameters::ro<6>(&version));
    /// node.set_params(TABLE);
    /// node.set_param_event_handler({&on_param, nullptr});   // opsiyonel doğrulama/bildirim
    ///
    /// node.spin_once();   // loop() içinde
    /// @endcode
    template<u8 MAX_SUBS       = 16,
             u8 MAX_FRAME_DATA = core::wireframe::MAX_DATA_LEN,
             u8 MAX_RELIABLE   = 8>
    class Node {
        static_assert(MAX_SUBS > 0,   "MAX_SUBS en az 1 olmalı");
        static_assert(MAX_SUBS < 254, "MAX_SUBS 254'ten küçük olmalı (ACK + PARAM_REQ için +2 slot)");

        // Dahili broker'da +2 slot: ACK kanalı (reliability) + PARAM_REQ (parameters).
        using NodeT     = RawNode<static_cast<u8>(MAX_SUBS + 2u), MAX_FRAME_DATA>;
        using ReliableT = overlays::reliability::Reliable<NodeT, MAX_RELIABLE, MAX_RELIABLE>;
        // Logger yalnızca PUBLISH eder (sink değil) → broker subscriber slotu
        // tüketmez ve zero-buffer'dır (NodeT* + Level). Log almak için host
        // tarafında logging::LogSink standalone kullanılır (minrospy Python sink).
        using LoggerT   = overlays::logging::Logger<NodeT, MAX_FRAME_DATA>;
        // Params, PARAM_REQ'e abone olur (1 broker slotu) ve PARAM_RES'ten yanıt
        // yollar. Node facade'ında best-effort'tur (node_ üzerinden); güvenilir
        // parametre isteyen standalone Params'ı reliability overlay'i üstüne kurar.
        // Tablo boş kurulur, set_params() ile flash constexpr tabloya bağlanır.
        using ParamsT   = overlays::parameters::Params<NodeT>;

    public:
        /// @brief Log seviyeleri (@ref overlays::logging::Level takma adı).
        using LogLevel = overlays::logging::Level;

        /// @brief Parametre event fazları (@ref overlays::parameters::Event takma adı).
        using ParamEvent        = overlays::parameters::Event;
        /// @brief Parametre event handler imzası.
        using ParamEventHandler = typename ParamsT::EventHandler;

        /// @brief Tipli callback: `delegate<void, const MsgT&>`.
        /// fn imzası: `void fn(const MsgT& msg, void* ctx)`.
        template<typename MsgT>
        using TypedCallback = utils::delegate<void, const MsgT&>;


        /// @brief `create_publisher<MsgT>()` tarafından döndürülen tipli yayıncı sarmalayıcısı.
        ///
        /// Reliable ise mesajı serileştirip kendi `buf_`'unda tutar (retransmit
        /// backing); `Reliable` bu buffer'ın pointer'ını tutar. Bu yüzden:
        /// @warning Kopyalanamaz (kopya farklı adres → dangling). Taşınabilir,
        ///          AMA yalnızca uçuşta mesaj YOKKEN (örn. setup'ta atama).
        template<typename MsgT>
        struct Publisher {
            Publisher() = default;

            Publisher(const Publisher&)            = delete;
            Publisher& operator=(const Publisher&) = delete;
            Publisher(Publisher&&)                 = default;
            Publisher& operator=(Publisher&&)      = default;

            /// @brief Mesajı yayınlar.
            /// @param msg Gönderilecek tipli mesaj.
            /// @return Reliable ise önceki mesaj hâlâ uçuştaysa (`can_send` false), ya da
            ///         `Publisher` geçersizse (`is_valid() == false`) `false` döner.
            bool publish(const MsgT& msg) {
                if (!hl_) return false;

                if (reliable_) {
                    if (!hl_->reliable_.can_send(ch_id_)) return false;  // uçuşta
                    msg.to_bytes(buf_);                                  // kalıcı backing
                    return hl_->reliable_.publish(ch_id_, buf_, MsgT::SIZE);
                }

                u8 buf[MsgT::SIZE];                                      // unreliable: stack yeterli
                msg.to_bytes(buf);
                return hl_->node_.publish(ch_id_, buf, MsgT::SIZE);
            }

            /// @brief `create_publisher()` başarıyla kanal kaydettiyse `true`.
            bool is_valid() const { return hl_ != nullptr; }

        private:
            friend class Node;
            Publisher(Node* hl, u8 id, bool reliable)
                : hl_(hl), ch_id_(id), reliable_(reliable) {}

            Node* hl_       = nullptr;
            u8      ch_id_    = 0;
            bool    reliable_ = false;
            u8      buf_[MsgT::SIZE]{};   // reliable retransmit için kalıcı backing
        };


        // ── Kurucu ───────────────────────────────────────────────────────
        Node() : transport(node_.transport), reliable_(node_), logger_(node_),
                 params_(node_) {}

        Node(const Node&)            = delete;
        Node& operator=(const Node&) = delete;


        // ── API ───────────────────────────────────────────────────────────

        /// @brief `node.transport = { ... };` ile doğrudan atama yapılan IO callback demeti.
        Transport& transport;

        /// @brief Ham node'u ve reliability timeout/retransmit mantığını ilerletir. Ana döngüde çağrılır.
        void spin_once() {
            node_.spin_once();
            reliable_.tick();
        }

        /// @brief Tipli yayıncı oluşturur.
        /// @tparam MsgT   Yayınlanacak mesaj tipi.
        /// @param ch_id   Kanal kimliği.
        /// @param reliable `true` ise güvenilir publisher kanalı kaydedilir.
        /// @return Kayıt başarısızsa (`reliable` istendi ama slot yoksa) geçersiz bir `Publisher`.
        template<typename MsgT>
        Publisher<MsgT> create_publisher(u8 ch_id, bool reliable = false) {
            if (reliable && !reliable_.register_pub(ch_id)) return {};
            return Publisher<MsgT>(this, ch_id, reliable);
        }

        /// @brief Tipli abonelik oluşturur.
        /// @tparam MsgT    Beklenen mesaj tipi.
        /// @param ch_id    Kanal kimliği.
        /// @param cb       Mesaj geldiğinde çağrılacak tipli callback.
        /// @param reliable `true` ise güvenilir subscriber olarak kaydedilir.
        /// @return `cb` geçersizse veya tipli abonelik slotu (`MAX_SUBS`) doluysa `false`.
        template<typename MsgT>
        bool create_subscription(u8 ch_id, TypedCallback<MsgT> cb, bool reliable = false) {
            if (!cb.is_valid())               return false;
            if (typed_sub_count_ >= MAX_SUBS) return false;

            TypedSubEntry& e = typed_subs_[typed_sub_count_++];
            e.dispatch = &TypedSubEntry::template make_dispatch<MsgT>;

            // Tipli fonksiyon pointer'ını void* olarak sakla.
            // sizeof(fn*) == sizeof(void*) tüm hedef platformlarda (ARM Cortex-M, ESP32, x86).
            auto fn = cb.raw_fn();
            memcpy(&e.fn, &fn, sizeof(e.fn));
            e.ctx = cb.raw_ctx();

            if (reliable)
                return reliable_.subscribe(ch_id, {&TypedSubEntry::raw_adapter, &e});
            return node_.subscribe(ch_id, {&TypedSubEntry::raw_adapter, &e});
        }


        // ── Logging (best-effort, CH248) ──────────────────────────────────
        //
        // Yalnızca yayın; zero-buffer. min_level altındaki çağrılar wire'a hiç
        // dokunmadan döner. String literal flash'ta kalır (memory-mapped flash
        // hedeflerinde kopya yok). Uzun mesaj otomatik parçalanır.

        /// @brief Eşik seviyesini ayarlar: bu seviyenin altındaki loglar bastırılır.
        void set_log_level(LogLevel level) { logger_.set_min_level(level); }

        /// @brief Ham byte log (binary-safe).
        /// @param level Log seviyesi.
        /// @param msg   Log verisi.
        /// @param len   `msg` uzunluğu.
        void log(LogLevel level, const u8* msg, u8 len) { logger_.log(level, msg, len); }

        /// @brief C-string kolaylığı: DEBUG seviyesinde log.
        void log_debug(const char* s) { logger_.debug(s); }
        /// @brief C-string kolaylığı: INFO seviyesinde log.
        void log_info (const char* s) { logger_.info(s);  }
        /// @brief C-string kolaylığı: WARN seviyesinde log.
        void log_warn (const char* s) { logger_.warn(s);  }
        /// @brief C-string kolaylığı: ERROR seviyesinde log.
        void log_error(const char* s) { logger_.error(s); }
        /// @brief C-string kolaylığı: FATAL seviyesinde log.
        void log_fatal(const char* s) { logger_.fatal(s); }


        // ── Parameters (get/set, CH247 REQ / CH246 RES) ───────────────────
        //
        // Parametre tablosu derleme-zamanı constexpr'dır (flash'ta yaşar, RAM
        // tüketmez); overlays::parameters::table(rw<>/ro<>...) ile kurulur.
        // storage'lar statik ömürlü olmalı (constexpr adres kısıtı). Host GET/SET
        // yaptıkça değişkenler doğrudan güncellenir. Best-effort'tur (bkz. ParamsT).

        /// @brief Derleme-zamanı parametre tablosunu bağlar.
        ///
        /// @code
        /// constexpr auto TABLE = overlays::parameters::table(
        ///     overlays::parameters::rw<5>(&kp),
        ///     overlays::parameters::ro<6>(&version));
        /// node.set_params(TABLE);
        /// @endcode
        ///
        /// @tparam N   Tablodaki giriş sayısı.
        /// @param table @ref overlays::parameters::table ile kurulan constexpr tablo.
        template<u8 N>
        void set_params(const overlays::parameters::ParamTable<N>& table) {
            params_.bind_table(table);
        }

        /// @brief Opsiyonel SET doğrulama (BEFORE_SET) + değişim bildirimi (AFTER_SET) handler'ı ayarlar.
        ///
        /// fn imzası: `bool(u8 id, ParamEvent ev, const u8* bytes, u8 len, void* ctx)`.
        ///
        /// @param h Event handler.
        void set_param_event_handler(ParamEventHandler h) {
            params_.set_event_handler(h);
        }

    private:
        // ── Tip silme (type erasure) — abonelik için ─────────────────────
        //
        // Her TypedSubEntry, MsgT tipini bilen bir dispatch fonksiyonu saklar.
        // Broker/Reliable: raw byte → raw_adapter → dispatch → deserialize → tipli callback.
        //
        struct TypedSubEntry {
            void (*dispatch)(const u8* payload, u8 len, void* fn, void* ctx) = nullptr;
            void* fn  = nullptr;
            void* ctx = nullptr;

            template<typename MsgT>
            static void make_dispatch(const u8* payload, u8 len, void* fn, void* ctx) {
                MsgT msg;
                if (!msg.from_bytes(payload, len)) return;
                void (*typed_fn)(const MsgT&, void*);
                memcpy(&typed_fn, &fn, sizeof(typed_fn));
                typed_fn(msg, ctx);
            }

            // RawNode/Reliable ChannelCallback imzası: fn(payload, len, ctx)
            static void raw_adapter(u8* payload, u8 len, void* self) {
                auto* e = static_cast<TypedSubEntry*>(self);
                if (e->dispatch) e->dispatch(payload, len, e->fn, e->ctx);
            }
        };

        NodeT         node_;
        ReliableT     reliable_;
        LoggerT       logger_;
        ParamsT       params_;
        TypedSubEntry typed_subs_[MAX_SUBS]{};
        u8            typed_sub_count_ = 0;
    };

} // namespace minros
