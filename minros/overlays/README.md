# Overlays {#minros-overlays}

**Overlay**, minros çekirdeğinin (core) *üzerine* oturan ama çekirdeğe hiçbir şey
eklemeyen bağımsız bir protokol katmanıdır. Her overlay yalnızca `RawNode`'un
public `publish`/`subscribe` API'sini kullanan sıradan bir "kullanıcı"dır —
framer, parser, broker ve wire formatı overlay'lerden habersizdir.

Bu klasördeki katmanlar:

| Overlay | Kanal | Durum | Ne yapar |
|---|---|---|---|
| \subpage minros-overlays-reliability "reliability" | **249** (ACK) | mevcut | ACK + retransmit ile güvenilir teslim (stop-and-wait, window=1) |
| \subpage minros-overlays-logging "logging" | **248** | mevcut | Seviyeli, best-effort log yayını + parça birleştirme (sink) |
| \subpage minros-overlays-parameters "parameters" | **247** (REQ) / **246** (RES) | mevcut | Düğüm parametrelerini host'tan oku/yaz (get/set), derleme-zamanı tablo |

Namespace / paket:
- C++ : `minros::overlays::reliability`, `minros::overlays::logging`, `minros::overlays::parameters`
- Python : `minrospy.overlays.reliability`, `minrospy.overlays.logging`, `minrospy.overlays.parameters`

---

## Overlay sözleşmesi

Bir katmanın "overlay" sayılması için uyduğu ortak kurallar:

### 1. Core'a dokunma
Overlay, `RawNode`'un public API'si dışında hiçbir çekirdek yapısını değiştirmez.
Böylece çekirdek küçük ve tek sorumluluklu kalır; overlay olmadan da RawNode tek
başına çalışır. `Node` (tipli yüksek seviye facade) overlay'leri sarmalar ama
onlara mecbur değildir.

### 2. Rezerve kanal + opak head öneki
Overlay'in kendi meta verisi, kullanıcı payload'ının önüne **opak bir head öneki**
olarak konur; core bu öneki bilmez, yalnızca overlay'in alıcı tarafı ayıklar.
Overlay'ler arası kanal çakışmasını önlemek için üst blok rezerve edilmiştir:

```
Rezerve protokol kanal bloğu
    249       = reliability ACK
    248       = logging
    247 / 246 = parameters (REQ / RES)
    ...       (yeni overlay'ler buradan aşağı doğru)
```

Kullanıcı kanalları bu bloğa girmemelidir.

### 3. Statik + kullanılmazsa sıfır maliyet
Overlay'ler template parametreleriyle (C++) / kurucu argümanlarıyla (Python)
boyutlanır; heap ve sanal dispatch kullanmaz. Kullanılmayan bir overlay ideal
olarak RAM/flash maliyeti getirmez (ör. `logging::Logger` yayıncısı zero-buffer;
reassembly buffer'ı yalnızca `LogSink`'te; `parameters` registry'si `.rodata`'da
yaşar, RAM tüketmez).

### 4. İki dilde simetri
C++ (`minros`) ve Python (`minrospy`) portları aynı wire formatını üretir. Bir
overlay eklerken iki tarafı da birlikte güncelle; `conformance/` vektörleriyle
uyum doğrulanabilir.

---

## Yeni bir overlay eklemek

1. Rezerve bloktan bir kanal seç (246'nın altından) ve buradaki tabloya ekle.
2. Meta verini **opak head öneki** olarak tasarla; core'a alan ekleme.
3. `RawNode`'a takılan bağımsız bir sınıf yaz (`publish`/`subscribe` kullanır).
   Kullanılmadığında maliyet getirmeyecek şekilde publisher/sink'i ayır.
4. C++ ve Python portlarını birlikte yaz; aynı wire formatını üret.
5. İstersen `Node` facade'ına ince bir API ekle (reliability/logging/parameters gibi).
