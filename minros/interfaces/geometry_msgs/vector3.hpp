#pragma once
#include <cstring>
#include "minros/interfaces/msg_base.hpp"

/// @file vector3.hpp
/// @brief Vector3 — 3 boyutlu float vektör mesajı (geometry_msgs).

namespace minros::interfaces::geometry_msgs {

/// @brief 3 boyutlu float vektör (x, y, z) — 12 byte, little-endian wire.
struct Vector3 : MsgBase<Vector3> {

    // MsgBase'in from_bytes/to_bytes'i private deserialize/serialize'i çağırabilsin
    friend struct MsgBase<Vector3>;

    static constexpr u8  SIZE      = 3u * sizeof(float);  ///< 12 byte.
    static constexpr u8  FAMILY_ID = 0x01;   ///< geometry_msgs ailesi.
    static constexpr u8  TYPE_ID   = 0x00;   ///< geometry_msgs-yerel: VECTOR3.

    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

private:
    // Doğrudan çağırma engellendi — sadece MsgBase::from_bytes üzerinden erişilir
    // Böylece size check her zaman yapılmış olur
    void deserialize(const u8* buf) noexcept {
        x = utils::endian::load_le<float>(buf);
        y = utils::endian::load_le<float>(buf +     sizeof(float));
        z = utils::endian::load_le<float>(buf + 2 * sizeof(float));
    }

    void serialize(u8* buf) const noexcept {
        utils::endian::store_le<float>(buf,                     x);
        utils::endian::store_le<float>(buf +     sizeof(float), y);
        utils::endian::store_le<float>(buf + 2 * sizeof(float), z);
    }
};

static_assert(sizeof(Vector3) == Vector3::SIZE, "Vector3: beklenmedik padding!");

}  // namespace minros::interfaces::geometry_msgs
