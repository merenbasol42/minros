#pragma once



#include <type_traits>


/// @file delegate.hpp
/// @brief delegate — heap ve virtual dispatch olmadan tip-güvenli callback sarmalayıcısı.

namespace minros::utils {

/// @brief Fonksiyon işaretçisi + opak context'ten oluşan hafif callback sarmalayıcısı.
///
/// `new`/`malloc` ve `virtual` kullanmaz; her instance yalnızca bir fonksiyon
/// işaretçisi ve bir `void*` tutar.
///
/// @tparam Ret  Dönüş tipi (varsayılan `void`).
/// @tparam Args Argüman tipleri.
template<typename Ret = void, typename... Args>
class delegate {
public:
    /// @brief Sarmalanan fonksiyon imzası: kullanıcı argümanları + context işaretçisi.
    using Fn = Ret (*)(Args..., void*);

    delegate() = default;
    /// @brief `fn`'i `obj` context'iyle sarmalar.
    delegate(Fn fn, void* obj) : fn(fn), obj(obj) {}

    /// @brief Sarmalanan fonksiyonu çağırır. `is_valid() == false` ise `Ret{}` döner (void hariç).
    Ret operator()(Args... args) const {
        if (fn) return fn(args..., obj);
        if constexpr (!std::is_void_v<Ret>) return Ret{};
    }

    /// @brief Bir fonksiyona bağlıysa `true`.
    bool is_valid() const { return fn != nullptr; }

    /// @brief Sarmalanan ham fonksiyon işaretçisi.
    Fn    raw_fn()  const { return fn; }
    /// @brief Sarmalanan ham context işaretçisi.
    void* raw_ctx() const { return obj; }

private:
    Fn    fn  = nullptr;
    void* obj = nullptr;
};

} // namespace minros::utils
