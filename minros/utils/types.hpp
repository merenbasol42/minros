#pragma once



#include <cstdint>
#include <type_traits>


/// @file types.hpp
/// @brief minros'ta kullanılan sabit-genişlikli tamsayı ve float takma adları.

namespace minros {

using u32 = uint32_t;
using u16 = uint16_t;
using u8  = uint8_t;

using i32 = int32_t;
using i16 = int16_t;
using i8  = int8_t;

/// @brief `T`'nin boyutu `Size` bayta eşit mi (compile-time kontrol için yardımcı).
template <typename T, u32 Size>
struct is_size : std::bool_constant<sizeof(T) == Size> {};

/// @brief 4 bayt genişliğinde float tipi (platformda `float` ya da `double` olabilir).
using f32 = std::conditional_t<
    is_size<float,4>::value, float,
    std::conditional_t<
        is_size<double,4>::value, double,
        void
    >
>;

static_assert(!std::is_same_v<f32, void>, "f32 bulunamadi");

/// @brief 8 bayt genişliğinde float tipi (platformda `double` ya da `long double` olabilir).
using f64 = std::conditional_t<
    is_size<double,8>::value, double,
    std::conditional_t<
        is_size<long double,8>::value, long double,
        void
    >
>;

// compile-time garanti
static_assert(!std::is_same_v<f64, void>, "f64 bulunamadi");

} // namespace minros
