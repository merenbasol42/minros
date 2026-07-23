# Reliability {#minros-overlays-reliability}

Publisher ACK gelene kadar yeni mesaj göndermez (`can_send` false); ACK gelmezse
`tick()` timeout'ta payload'ı otonom yeniden gönderir (kopya tutar / pointer
tutar, retransmit callback'i yoktur). Subscriber tarafı dedup + otomatik ACK
yapar. Head öneki: 1 baytlık `SEQ`.

Wire kontratı, tasarım gerekçesi ve akış: \subpage minros-overlays-reliability-protocol
"reliability-protocol.md".

Header Doxygen: [`reliable.hpp`](reliable.hpp), [`reliability_protocol.hpp`](reliability_protocol.hpp).
