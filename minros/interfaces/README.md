# Interfaces (Arayüzler) {#minros-interfaces}

**Mesaj**, `MsgBase` (CRTP) taban sınıfından türeyen, sabit boyutlu, little-endian
serileştirilebilir bir veri tipidir. Heap ve virtual dispatch kullanmaz;
`from_bytes`/`to_bytes` derleme-zamanında ilgili tipe bağlanır (bkz.
`msg_base.hpp`).

Bu klasördeki aileler:

| Aile | Namespace | Ne taşır |
|---|---|---|
| \subpage minros-interfaces-std-msgs "std_msgs" | `minros::interfaces::std_msgs` | İlkel skaler tipler (Float32, Int32, Int16, Int8, UInt32, UInt16, UInt8, Bool) + PidGains |
| \subpage minros-interfaces-geometry-msgs "geometry_msgs" | `minros::interfaces::geometry_msgs` | Vector3, Quaternion, Twist |

Namespace / paket:
- C++ : `minros::interfaces::std_msgs`, `minros::interfaces::geometry_msgs`
- Python : `minrospy.interfaces.std_msgs`, `minrospy.interfaces.geometry_msgs`

---

## Mesaj sözleşmesi

Bir tipin "mesaj" sayılması için uyduğu ortak kurallar:

### 1. CRTP taban sınıfı
`struct Foo : MsgBase<Foo> { ... };` — virtual/heap yok; `from_bytes`/`to_bytes`
derleme-zamanında `Foo`'ya bağlanır (indirect call / vptr maliyeti yok).

### 2. Sabit boyut + LE serileştirme
Her tip `static constexpr u8 SIZE` taşır ve private `serialize(u8*) const` /
`deserialize(const u8*)` sağlar; alanlar `utils::endian::store_le` /
`load_le` ile little-endian yazılır/okunur.
`static_assert(sizeof(T) == T::SIZE, ...)` padding olmadığını derleme-zamanında
garanti eder.

### 3. İki parçalı tip kimliği
`FAMILY_ID` (aile — `0x00`–`0x7F` resmi/rezerve, `0x80`–`0xFF` özel kullanım) +
`TYPE_ID` (aile-yerel, aile içinde çakışmasız olmalı). Ayrıntı: `msg_base.hpp`
Doxygen (`family_id()` / `type_id()`).

### 4. İki dilde simetri
C++ (`minros`) ve Python (`minrospy`) temsilleri aynı wire byte'larını
üretir/tüketir. Yeni tip eklerken iki tarafı da birlikte güncelle;
`conformance/` vektörleriyle uyum doğrulanabilir.

---

## Ailelerin özeti

### std_msgs — FAMILY_ID `0x00`

| Tip | TYPE_ID | Boyut |
|---|---|---|
| `Float32` | `0x00` | 4 |
| `Int32` | `0x01` | 4 |
| `Int16` | `0x02` | 2 |
| `Int8` | `0x03` | 1 |
| `UInt32` | `0x04` | 4 |
| `UInt16` | `0x05` | 2 |
| `UInt8` | `0x06` | 1 |
| `Bool` | `0x07` | 1 |
| `PidGains` | `0x0B` | 12 |

Ayrıntı: \subpage minros-interfaces-std-msgs "std_msgs" — header Doxygen
(`primitives.hpp`, `pid_gains.hpp`).

### geometry_msgs — FAMILY_ID `0x01`

| Tip | TYPE_ID | Boyut |
|---|---|---|
| `Vector3` | `0x00` | 12 |
| `Quaternion` | `0x01` | 16 |
| `Twist` | `0x02` | 24 |

Ayrıntı: \subpage minros-interfaces-geometry-msgs "geometry_msgs" — header
Doxygen (`vector3.hpp`, `quaternion.hpp`, `twist.hpp`).

---

## Yeni bir mesaj tipi eklemek

1. Uygun aileyi seç; yoksa yeni bir aile aç ve rezerve olmayan bir `FAMILY_ID`
   tahsis edip yukarıdaki tabloya ekle.
2. `struct Foo : MsgBase<Foo> { ... };` yaz: `friend struct MsgBase<Foo>;`,
   `SIZE`/`FAMILY_ID`/`TYPE_ID` sabitleri, alanlar.
3. Private `serialize`/`deserialize`'ı `utils::endian::store_le`/`load_le`
   üzerinden yaz.
4. `static_assert(sizeof(Foo) == Foo::SIZE, "...")` ekle (padding kontrolü).
5. Header'ı repo kökündeki `docs/DOCUMENTATION.md` Kural 1 ile Doxygen'le
   belgele.
6. Python (`minrospy`) karşılığını aynı wire formatıyla yaz; `conformance/`
   vektörüne ekle.
