#include <Arduino.h>

#include <minros/raw_node.hpp>
#include <minros/node.hpp>
#include <minros/overlays/reliability/reliable.hpp>
#include <minros/overlays/parameters/params.hpp>
#include <minros/interfaces/geometry_msgs/vector3.hpp>
#include <minros/interfaces/std_msgs/primitives.hpp>

// Tipler (u8/u32) ve delegate artık minros namespace'i altında (bkz. 56b2db6).
using namespace minros;

// ─────────────────────────────────────────────────────────────────────────────
// Düğüm tipi seçimi — YALNIZCA birini seç.
//
//   NODE_TYPE_HL   : tipli yüksek seviye API (minros::Node)
//   NODE_TYPE_RAW  : ham byte API (minros::RawNode) + reliability::Reliable overlay
//
// İki mod da DIŞARIDAN birebir aynı davranır (aynı 4 kanal, aynı Vector3 echo);
// yalnızca içte kullanılan API katmanı değişir. Böylece aynı Python test
// betikleri hem ham hem tipli yolu doğrular.
// ─────────────────────────────────────────────────────────────────────────────
#define NODE_TYPE_HL   0
#define NODE_TYPE_RAW  1

#define NODE_TYPE  NODE_TYPE_HL    // ← düğüm tipini buradan değiştir


// ─── Kanal şeması (cihaz perspektifi) ────────────────────────────────────────
//   CH 0: unreliable sub  (PC publish → cihaz alır)
//   CH 1: unreliable pub  (cihaz echo → PC alır)
//   CH 2: reliable   sub  (PC reliable publish → cihaz alır)
//   CH 3: reliable   pub  (cihaz reliable echo → PC alır)
static constexpr u8 CH_UNREL_SUB = 0;
static constexpr u8 CH_UNREL_PUB = 1;
static constexpr u8 CH_REL_SUB   = 2;
static constexpr u8 CH_REL_PUB   = 3;

using Vector3 = minros::interfaces::geometry_msgs::Vector3;
namespace std_msgs = minros::interfaces::std_msgs;

// ─── Parametreler — host PARAM_REQ/RES üzerinden get/set eder ─────────────────
//   id 0  gain       Vector3   rw   echo çarpanı (eksen-başına). Varsayılan
//                                    (2,2,2) → eski ×2 davranışı (Python testleri).
//   id 1  max_speed  Float32   rw   örnek skaler, [0, 100] ile sınırlanır.
//   id 2  fw_version UInt32    ro   salt-okunur; SET denenirse READ_ONLY döner.
static constexpr u8 PARAM_GAIN      = 0;
static constexpr u8 PARAM_MAX_SPEED = 1;
static constexpr u8 PARAM_FW_VER    = 2;

// Storage'lar statik ömürlü olmalı (constexpr tablo &var'ları sabit-ifade ister).
static Vector3          gain;
static std_msgs::Float32 max_speed;
static std_msgs::UInt32  fw_version;

// Parametre tablosu — derleme-zamanı constexpr → .rodata (flash), RAM tüketmez.
static constexpr auto PARAM_TABLE = overlays::parameters::table(
    overlays::parameters::rw<PARAM_GAIN>(&gain),
    overlays::parameters::rw<PARAM_MAX_SPEED>(&max_speed),
    overlays::parameters::ro<PARAM_FW_VER>(&fw_version));

// Echo dönüşümü: her bileşeni gain ile çarpar. (Sabit ×2 yerine çalışma-anı
// parametresi — Python testleri payload == girdi*gain bekler. Varsayılan gain
// (2,2,2) eski davranışı korur; host set ile değiştirilebilir.)
static Vector3 echo_of(const Vector3& m) {
    Vector3 o;
    o.x = m.x * gain.x;
    o.y = m.y * gain.y;
    o.z = m.z * gain.z;
    return o;
}


// ─── Parametre event handler'ı (tek callback, tüm paramlar) ──────────────────
//
// Overlay başına TEK delegate; her SET'te iki fazla çağrılır ve id'ye göre
// dallanır (klasik CANopen-tarzı doğrulama sözlüğü):
//   BEFORE_SET : önerilen değer henüz yazılmadı. false → storage'a YAZILMAZ,
//                host'a ERR/REJECTED gider (eski değer korunur). Aralık/clamp
//                mantığı burada.
//   AFTER_SET  : değer yazıldı → bildirim / türetilmiş durumu tazeleme
//                (dönüş yok sayılır).
//
// Değeri okumak için reinterpret_cast DEĞİL from_bytes (memcpy tabanlı) kullan:
// bytes frame buffer'ının içine işaret eder, hizalı olma garantisi yoktur
// (Cortex-M0'da unaligned erişim fault). from_bytes hizalama + endian güvenli.
static constexpr float GAIN_ABS_MAX  = 8.0f;     // |gain bileşeni| ≤ 8
static constexpr float MAX_SPEED_CAP = 100.0f;   // max_speed ∈ [0, 100]

static u32 gain_change_count = 0;

static bool on_param(u8 id, overlays::parameters::Event ev,
                     const u8* bytes, u8 len, void*) {
    using Event = overlays::parameters::Event;

    switch (id) {
    case PARAM_GAIN:
        if (ev == Event::BEFORE_SET) {
            Vector3 v;
            if (!v.from_bytes(bytes, len)) return false;
            return v.x >= -GAIN_ABS_MAX && v.x <= GAIN_ABS_MAX &&
                   v.y >= -GAIN_ABS_MAX && v.y <= GAIN_ABS_MAX &&
                   v.z >= -GAIN_ABS_MAX && v.z <= GAIN_ABS_MAX;
        }
        gain_change_count++;    // AFTER_SET: gain değişti bildirimi
        return true;

    case PARAM_MAX_SPEED:
        if (ev == Event::BEFORE_SET) {
            std_msgs::Float32 v;
            if (!v.from_bytes(bytes, len)) return false;
            return v.value >= 0.0f && v.value <= MAX_SPEED_CAP;
        }
        return true;            // AFTER_SET: özel işlem yok

    default:
        return true;            // kuralı olmayan paramlar serbest geçer
    }
    // Not: fw_version salt-okunur → SET overlay'de READ_ONLY ile daha handler'a
    // gelmeden reddedilir; burada case'i yok.
}


// ─── Transport (tek Serial, tek düğüm) ───────────────────────────────────────

static u8   tp_get_size   (void*)               { return static_cast<u8>(Serial.available()); }
static u32  tp_get_time   (void*)               { return millis(); }
static void tp_read_bytes (u8* b, u8 n, void*)  { Serial.readBytes(b, n); }
static void tp_write_bytes(u8* b, u8 n, void*)  { Serial.write(b, n); }

static minros::Transport serial_transport = {
    .send_bytes = { tp_write_bytes, nullptr },
    .read_bytes = { tp_read_bytes,  nullptr },
    .get_size   = { tp_get_size,    nullptr },
    .get_time   = { tp_get_time,    nullptr },
};


// ═════════════════════════════════════════════════════════════════════════════
//  NODE_TYPE_HL — tipli Node ile echo
// ═════════════════════════════════════════════════════════════════════════════
#if NODE_TYPE == NODE_TYPE_HL

static minros::Node<>                       node;
static minros::Node<>::Publisher<Vector3>   unrel_pub;   // CH 1
static minros::Node<>::Publisher<Vector3>   rel_pub;     // CH 3

static void on_unrel(const Vector3& msg, void*) {
    unrel_pub.publish(echo_of(msg));               // best-effort echo
}

static void on_rel(const Vector3& msg, void*) {
    rel_pub.publish(echo_of(msg));                 // reliable echo (uçuştaysa drop'lanır)
}

void setup() {
    Serial.begin(115200);
    node.transport = serial_transport;

    gain.x = gain.y = gain.z = 2.0f;                  // varsayılan ×2
    max_speed.value = 50.0f;                          // varsayılan skaler param
    fw_version.value = 0x00010203;                    // salt-okunur sürüm damgası
    node.set_params(PARAM_TABLE);                     // Node facade param sunucusu (flash tablo)
    node.set_param_event_handler({ on_param, nullptr });  // aralık kontrolü + bildirim

    unrel_pub = node.create_publisher<Vector3>(CH_UNREL_PUB);
    rel_pub   = node.create_publisher<Vector3>(CH_REL_PUB, /*reliable=*/true);

    node.create_subscription<Vector3>(CH_UNREL_SUB, { on_unrel, nullptr });
    node.create_subscription<Vector3>(CH_REL_SUB,   { on_rel,   nullptr }, /*reliable=*/true);
}

void loop() {
    node.spin_once();   // parser + reliable tick birlikte
}


// ═════════════════════════════════════════════════════════════════════════════
//  NODE_TYPE_RAW — ham RawNode + reliability::Reliable overlay ile echo
// ═════════════════════════════════════════════════════════════════════════════
#elif NODE_TYPE == NODE_TYPE_RAW

static minros::RawNode<>                 node;
static minros::overlays::reliability::Reliable  rel{ node };   // aynı node'a takılır (ACK kanalına abone)
static minros::overlays::parameters::Params     params{ node, PARAM_TABLE };  // PARAM_REQ'e abone (CTAD)

// Reliable publisher buffer'ı ACK gelene kadar SABİT kalmalı (Reliable pointer tutar).
static u8 rel_tx[Vector3::SIZE];

// Unreliable: CH 0 → CH 1
static void on_unrel_bytes(u8* payload, u8 len, void*) {
    Vector3 in;
    if (!in.from_bytes(payload, len)) return;
    Vector3 out = echo_of(in);

    u8 buf[Vector3::SIZE];
    minros::interfaces::serialize_to(out, buf);
    node.publish(CH_UNREL_PUB, buf, Vector3::SIZE);
}

// Reliable: CH 2 → CH 3 (callback seq önekı ayıklanmış payload alır)
static void on_rel_bytes(u8* payload, u8 len, void*) {
    Vector3 in;
    if (!in.from_bytes(payload, len)) return;
    Vector3 out = echo_of(in);

    if (rel.can_send(CH_REL_PUB)) {                 // önceki echo ACK'lendi mi?
        minros::interfaces::serialize_to(out, rel_tx);
        rel.publish(CH_REL_PUB, rel_tx, Vector3::SIZE);
    }
}

void setup() {
    Serial.begin(115200);
    node.transport = serial_transport;

    gain.x = gain.y = gain.z = 2.0f;                  // varsayılan ×2
    max_speed.value = 50.0f;                          // varsayılan skaler param
    fw_version.value = 0x00010203;                    // salt-okunur sürüm damgası
    // params tablosu ctor'da bağlandı (PARAM_TABLE) — host get/set eder
    params.set_event_handler({ on_param, nullptr });  // aralık kontrolü + bildirim

    node.subscribe(CH_UNREL_SUB, { on_unrel_bytes, nullptr });   // best-effort
    rel.subscribe(CH_REL_SUB,    { on_rel_bytes,   nullptr });   // reliable (dedup + ACK)
    rel.register_pub(CH_REL_PUB);
}

void loop() {
    node.spin_once();   // gelen baytlar
    rel.tick();         // timeout / retransmit
}

#else
#error "NODE_TYPE: NODE_TYPE_HL veya NODE_TYPE_RAW olmalı"
#endif