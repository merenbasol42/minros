# Reliability overlay — ACK + retransmit ile güvenilir teslim {#minros-overlays-reliability-protocol}

durum: mevcut

## Amaç / Ne çözer

Reliability overlay'i, kayıplı bir link üzerinde bir mesajın **teslim edildiğinden
emin olmayı** ve aynı mesajın **iki kez işlenmemesini** sağlar. ROS'taki
"reliable QoS" ayarının gömülü-dostu karşılığıdır: ACK + timeout'ta yeniden
gönderim (retransmit) + alıcıda duplicate filtreleme. `Sequencer`'ın yerini alan
bu tasarımda güvenilirlik artık `RawNode`'un içinde değil, `RawNode`'un public
pub/sub API'sini kullanan bağımsız bir katmandır.

## Tasarım ilkeleri

### 1. Core'a dokunmaz — SEQ opak bir head önekidir

Wire frame'de SEQ alanı yoktur; core SEQ bilmez. `Reliable`, sequence numarasını
kullanıcı payload'ının önüne 1 baytlık opak bir önek olarak koyar
(`PAYLOAD = [SEQ][user bytes...]`). Core bunu sıradan payload görür; öneki
yalnızca `Reliable`'ın alıcı tarafı ayıklar.

### 2. RawNode'un sıradan bir kullanıcısıdır

Veriyi `node.publish` ile gönderir, ACK'i rezerve ACK kanalından (CH249,
[`overlays/README.md`](../README.md) kanal tablosu) yine `node.publish` ile
yollar, abonelikleri `node.subscribe` ile yapar. `Node` gerektirmez — ham
`RawNode` ile kullanılabilir. ACK kanalında her iki uç (host ve MCU) hem
publisher hem subscriber'dır.

### 3. Stop-and-wait (window = 1)

Kanal başına aynı anda en fazla 1 uçuştaki frame vardır. Publisher, ACK gelene
kadar o kanalda yeni mesaj gönderemez (`can_send` false döner). Bu, kanal başına
tek `seq`/ACK uzayıyla duplicate sorununu en düşük RAM maliyetiyle çözer.

### 4. Zero-copy / pointer-tutma

Payload kopyalanmaz; yalnızca `(const u8* + len + seq)` tutulur. Timeout olunca
`tick()` o pointer'dan **kendisi** yeniden gönderir. Eski tasarımdaki
"kullanıcıya geri talep eden retransmit callback" yoktur.

## Sınırlar

> **Sınır 1 — kanal başına tek publisher.** Kanal başına tek `seq`/ACK uzayı
> vardır; aynı kanala iki uçtan reliable yayın yapılamaz. Çift yönlü güvenilir
> trafik isteyen bir özellik, parameters overlay'inin yaptığı gibi yön başına
> ayrı kanal kullanır.

> **Sınır 2 — buffer ACK'e kadar sabit kalmalı.** `publish(ch, buf, len)`
> çağrıldıktan sonra, ACK gelene (`can_send(ch)` tekrar `true` olana) kadar
> `buf` bozulmamalıdır — `Reliable` yalnızca pointer'ını tutar ve retransmit'i
> oradan yapar.

> **Sınır 3 — teslim garantisi MAX_RETRY ile sınırlıdır.** `MAX_RETRY` deneme
> sonunda ACK gelmezse mesajdan vazgeçilir: kanal serbest bırakılır ve (varsa)
> hata callback'i `MAX_RETRIED` ile çağrılır. Sonsuz bloklama yoktur.

## Wire mesajları

Reliable veri, kullanıcının kendi kanalında taşınır; yalnızca payload'ın başına
SEQ öneki eklenir. ACK ise rezerve CH249'dan gider:

```
── Reliable veri (CH = kullanıcı kanalı) ──────────────────────────
DATA  : [SEQ][user bytes...]
        SEQ, kanal başına 1 baytlık dönen sayaçtır; core için opaktır.

── ACK (CH249, alıcı → gönderici) ─────────────────────────────────
ACK   : [RESP=0x06][CH_ID][SEQ]
        CH_ID : ACK'lenen kanal
        SEQ   : ACK'lenen mesajın sequence numarası
```

### RESP kodları

| RESP | Ad | Anlam |
|-----:|-----|-------|
| 0x06 | ACK | Mesaj alındı (ASCII ACK) |

> **Not —** Şu an yalnızca ACK vardır, NACK yoktur; `RESP` alanı ileride
> eklenebilsin diye ayrılmıştır.

## Davranış

**Publisher** (kanal başına durum):

1. `publish(ch, buf, len)` → seq artırılır, `[SEQ][payload]` yayılır,
   `ack_pending` set edilir, zaman damgası alınır.
2. `ack_pending` iken `can_send(ch)` false döner; yeni `publish` reddedilir
   (buffer'a dokunulmaz).
3. Doğru `(CH_ID, SEQ)` taşıyan ACK gelince `ack_pending` düşer, kanal serbest.
4. `tick()` her ana döngüde çağrılır: `TIMEOUT_MS` geçtiyse aynı seq ile aynı
   pointer'dan yeniden gönderir. `MAX_RETRY` aşılırsa vazgeçer ve hata
   callback'ini (`MAX_RETRIED`) çağırır.

**Subscriber**:

1. Gelen payload'dan SEQ ayıklanır.
2. **Her** mesaja (duplicate dahil) ACK gönderilir — duplicate gelmesi, önceki
   ACK'in kaybolduğu anlamına gelir; tekrar ACK'lemek döngüyü kapatır.
3. SEQ, son işlenenle aynıysa veri **bir daha işlenmez** (dedup); farklıysa
   kullanıcı callback'i seq öneki ayıklanmış veriyle çağrılır.

Zamanlama/deneme parametreleri (`MAX_RETRY`, `TIMEOUT_MS`) ve API imzaları
template parametresidir; ayrıntı için [`reliable.hpp`](reliable.hpp).

## Örnek akış

```
── Normal teslim ──────────────────────────────────────────────────
Pub  → CH7 :  DATA seq=1  [01][user bytes]
Sub  → 249 :  ACK         [06][07][01]
              can_send(7) tekrar true

── Veri frame'i düştü ─────────────────────────────────────────────
Pub  → CH7 :  DATA seq=2  ✕ (kayıp)
              ... TIMEOUT_MS ...
Pub  → CH7 :  DATA seq=2  (tick() aynı pointer'dan yeniden gönderdi)
Sub  → 249 :  ACK         [06][07][02]

── ACK düştü → duplicate ──────────────────────────────────────────
Pub  → CH7 :  DATA seq=3
Sub  → 249 :  ACK ✕ (kayıp)          ← veri işlendi ama ACK ulaşmadı
              ... TIMEOUT_MS ...
Pub  → CH7 :  DATA seq=3  (retransmit)
Sub  :        seq==last_seq → veri İŞLENMEZ, yalnızca ACK tekrarlanır
Sub  → 249 :  ACK         [06][07][03]
```

## Güvenilirlik / etkileşim

- **`Node` (tipli facade)**: buffer sabitliği sözleşmesini kullanıcıdan gizler —
  reliable `Publisher<MsgT>` mesajı kendi `buf_[MsgT::SIZE]` üyesine serileştirip
  onu retransmit backing olarak tutar; `publish(msg)` busy ise `false` döner
  (msg bozulmaz). Bkz. [`node.hpp`](../../node.hpp).
- **parameters**: SET/VALUE trafiğini bu overlay üzerinden güvenilir taşıyabilir;
  REQ/RES kanal ayrımı tam da tek-publisher sözleşmesi (Sınır 1) için yapılmıştır.
  Bkz. [`parameters-protocol.md`](../parameters/parameters-protocol.md).
- **logging**: bilinçli olarak bu overlay'e **sokulmaz** — log best-effort'tur,
  ana akışı bloklamaz. Bkz. [`logging-protocol.md`](../logging/logging-protocol.md).

## Kaynak maliyeti

Kanal başına publisher/subscriber slotu 16 byte'tır; sayılar için
[`maliyet.md`](../../maliyet.md).
