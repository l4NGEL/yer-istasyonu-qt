#pragma once
// Little-endian alan okuma/yazma. MAVLink yükü her zaman little-endian'dır;
// host endianness'ından bağımsız olsun diye bayt bayt kaydırma ile çalışır.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace gcs::mav {

namespace detail {
template <std::size_t N> struct UnsignedOf;
template <> struct UnsignedOf<1> { using type = std::uint8_t; };
template <> struct UnsignedOf<2> { using type = std::uint16_t; };
template <> struct UnsignedOf<4> { using type = std::uint32_t; };
template <> struct UnsignedOf<8> { using type = std::uint64_t; };
}  // namespace detail

class Writer {
public:
    explicit Writer(std::uint8_t* out) : p_(out) {}

    template <class T>
    void put(T value) {
        static_assert(std::is_arithmetic_v<T>, "yalnızca sayısal alanlar");
        using U = typename detail::UnsignedOf<sizeof(T)>::type;
        U u;
        std::memcpy(&u, &value, sizeof(T));
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            *p_++ = static_cast<std::uint8_t>(u >> (8 * i));
        }
    }

    void putBytes(const void* src, std::size_t n) {
        std::memcpy(p_, src, n);
        p_ += n;
    }

private:
    std::uint8_t* p_;
};

class Reader {
public:
    explicit Reader(const std::uint8_t* in) : p_(in) {}

    template <class T>
    T get() {
        static_assert(std::is_arithmetic_v<T>, "yalnızca sayısal alanlar");
        using U = typename detail::UnsignedOf<sizeof(T)>::type;
        U u = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            u = static_cast<U>(u | (static_cast<U>(*p_++) << (8 * i)));
        }
        T value;
        std::memcpy(&value, &u, sizeof(T));
        return value;
    }

    void getBytes(void* dst, std::size_t n) {
        std::memcpy(dst, p_, n);
        p_ += n;
    }

private:
    const std::uint8_t* p_;
};

}  // namespace gcs::mav
