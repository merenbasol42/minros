# Logging {#minros-overlays-logging}

Best-effort (unreliable) seviyeli log. Kaynakta `min_level` filtresi eşik altı
çağrıları wire'a hiç dokundurmaz. Uzun satır, küçük frame'lerde otomatik
parçalanır; sink `SEQ4` sürekliliğiyle kayıp parçayı tespit edip bozuk satır
üretmez. Head öneki: 1 baytlık `FLAGS` (`LAST | LEVEL | SEQ4`).

Wire kontratı, tasarım gerekçesi ve akış: \subpage minros-overlays-logging-protocol
"logging-protocol.md".

Header Doxygen: [`logger.hpp`](logger.hpp), [`logging_protocol.hpp`](logging_protocol.hpp).
