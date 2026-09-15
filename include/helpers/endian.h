#pragma once

#include <bit>

#include "dolphin/types.h"
#include "dolphin/mtx.h"

// Platform detection - Little Endian targets
#if defined(_WIN32) || defined(__x86_64__) || defined(__i386__) || defined(__aarch64__) || defined(_M_X64) || defined(_M_IX86)
    #define TARGET_LITTLE_ENDIAN 1
#else
    #define TARGET_LITTLE_ENDIAN 0
#endif

#if TARGET_LITTLE_ENDIAN
    #ifdef _MSC_VER
        #include <stdlib.h>
        #define BSWAP16(x) _byteswap_ushort(x)
        #define BSWAP32(x) _byteswap_ulong(x)
        #define BSWAP64(x) _byteswap_uint64(x)
    #else
        #define BSWAP16(x) __builtin_bswap16(x)
        #define BSWAP32(x) __builtin_bswap32(x)
        #define BSWAP64(x) __builtin_bswap64(x)
    #endif
#else
    #define BSWAP16(x) (x)
    #define BSWAP32(x) (x)
#endif

// clang-format off
// yeah ofc Microsoft's _byteswap_* aren't constexpr like the GCC/Clang ones.
constexpr u16 be16_manual(u16 val)
{
    return (val >> 8) |
           (val << 8);
}

constexpr u32 be32_manual(u32 val)
{
    return (val >> 24) |
          ((val << 8)  & 0x00FF0000) |
          ((val >> 8)  & 0x0000FF00) |
           (val << 24);
}

constexpr u64 be64_manual(u64 val)
{
    return (val >> 56) |
          ((val << 40) & 0x00FF000000000000) |
          ((val << 24) & 0x0000FF0000000000) |
          ((val << 8)  & 0x000000FF00000000) |
          ((val >> 8)  & 0x00000000FF000000) |
          ((val >> 24) & 0x0000000000FF0000) |
          ((val >> 40) & 0x000000000000FF00) |
           (val << 56);
}
// clang-format on

// Big-Endian to Host conversion
// _manual is behind std::is_constant_evaluated() checks,
// to avoid pessimizing debug perf too much.
constexpr u16 be16(u16 val) {
    if (std::is_constant_evaluated()) {
        return be16_manual(val);
    }
    return BSWAP16(val);
}
constexpr s16 be16s(s16 val) {
    if (std::is_constant_evaluated()) {
        return (s16)be16_manual((u16)val);
    }
    return (s16)BSWAP16((u16)val);
}
constexpr u32 be32(u32 val) {
    if (std::is_constant_evaluated()) {
        return be32_manual(val);
    }
    return BSWAP32(val);
}
constexpr s32 be32s(s32 val) {
    if (std::is_constant_evaluated()) {
        return (s32)be32_manual((u32)val);
    }
    return (s32)BSWAP32((u32)val);
}
constexpr u64 be64(u64 val) {
    if (std::is_constant_evaluated()) {
        return be64_manual(val);
    }
    return BSWAP64(val);
}
constexpr s64 be64s(s64 val) {
    if (std::is_constant_evaluated()) {
        return (s64)be64_manual((u64)val);
    }
    return (s64)BSWAP64((u64)val);
}

#ifdef TARGET_PC
// Helper wrappers so code below reads nicely:
constexpr u16 RES_U16(u16 v) {
    return be16(v);
}
constexpr s16 RES_S16(s16 v) {
    return be16s(v);
}
constexpr u32 RES_U32(u32 v) {
    return be32(v);
}
constexpr s32 RES_S32(s32 v) {
    return be32s(v);
}
constexpr u64 RES_U64(u64 v) {
    return be64(v);
}
constexpr s64 RES_S64(s64 v) {
    return be64s(v);
}
constexpr f32 RES_F32(f32 v) {
    return std::bit_cast<f32, s32>(RES_S32(std::bit_cast<s32, f32>(v)));
}
#else
// On GameCube host-endian == file-endian, these are no-ops (keep as macros to allow compile in
// original code paths)
#define RES_U16(x) (x)
#define RES_S16(x) (x)
#define RES_U32(x) (x)
#define RES_S32(x) (x)
#endif

#ifdef TARGET_PC

/*
 * Declares a big-endian integer type.
 */
template<class T>
struct BE {
    T inner;
    constexpr BE() noexcept = default;
    constexpr BE(const T& from) noexcept {
        inner = swap(from);
    }

    // post-ops
    constexpr T operator--(int) {
        T orig = inner;
        *this -= 1;
        return swap(orig);
    }

    constexpr T operator++(int) {
        T orig = inner;
        *this += 1;
        return swap(orig);
    }

    constexpr operator T() const noexcept {
        return swap(inner);
    }

    constexpr T host[[nodiscard]]() const noexcept {
        return swap(inner);
    }

    static constexpr T swap[[nodiscard]](T val) noexcept;
};

#define BIN_ASSIGN_OP(op)                          \
                                                   \
template<typename TA, typename TB>                 \
constexpr BE<TA>& operator op(BE<TA>& a, TB b) {   \
    TA aCopy = a;                                  \
    aCopy op b;                                    \
    a = aCopy;                                     \
    return a;                                      \
}

BIN_ASSIGN_OP(&=);
BIN_ASSIGN_OP(|=);
BIN_ASSIGN_OP(+=);
BIN_ASSIGN_OP(-=);
BIN_ASSIGN_OP(/=);
BIN_ASSIGN_OP(^=);

#undef BIN_ASSIGN_OP

template<>
constexpr u16 BE<u16>::swap(u16 val) noexcept {
    return RES_U16(val);
}

template<>
constexpr s16 BE<s16>::swap(s16 val) noexcept {
    return RES_S16(val);
}

template<>
constexpr u32 BE<u32>::swap(u32 val) noexcept {
    return RES_U32(val);
}

template<>
constexpr s32 BE<s32>::swap(s32 val) noexcept {
    return RES_S32(val);
}

template<>
constexpr s64 BE<s64>::swap(s64 val) noexcept {
    return RES_S64(val);
}

template<>
constexpr u64 BE<u64>::swap(u64 val) noexcept {
    return RES_U64(val);
}

template<>
constexpr f32 BE<f32>::swap(f32 val) noexcept {
    return RES_F32(val);
}

template<>
constexpr S16Vec BE<S16Vec>::swap(S16Vec val) noexcept {
    return {
        BE<s16>::swap(val.x),
        BE<s16>::swap(val.y),
        BE<s16>::swap(val.z),
    };
}

template<>
struct BE<Vec> {
    BE<f32> x;
    BE<f32> y;
    BE<f32> z;

    constexpr BE() noexcept = default;
    constexpr BE(f32 x, f32 y, f32 z) noexcept {
        this->x = x;
        this->y = y;
        this->z = z;
    }
    constexpr BE(const Vec& from) noexcept {
        x = from.x;
        y = from.y;
        z = from.z;
    }

    constexpr operator Vec() const noexcept {
        return { x, y, z };
    }

    constexpr static Vec swap(Vec val) noexcept {
        return {
            BE<f32>::swap(val.x),
            BE<f32>::swap(val.y),
            BE<f32>::swap(val.z),
        };
    }
};

template <>
struct BE<Mtx44> {
    BE<f32> contents[4][4];

    constexpr auto& operator[](int x) const noexcept {
        return contents[x];
    }
};

template <>
struct BE<Mtx> {
    BE<f32> contents[3][4];

    constexpr auto& operator[](int x) const noexcept {
        return contents[x];
    }

    constexpr void to_host(Mtx& mtx) const noexcept {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                mtx[i][j] = contents[i][j];
            }
        }
    }
};

typedef f32 Mtx23[2][3];
template <>
struct BE<Mtx23> {
    BE<f32> contents[2][3];

    constexpr auto& operator[](int x) noexcept {
        return contents[x];
    }

    constexpr auto& operator[](int x) const noexcept {
        return contents[x];
    }

    constexpr void to_host(Mtx23& mtx) const noexcept {
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 3; j++) {
                mtx[i][j] = contents[i][j];
            }
        }
    }
};

template<typename T>
constexpr void be_swap(T& val) noexcept {
    val = BE<T>::swap(val);
}

template<typename T, u32 N>
constexpr void be_swap(T (& val)[N]) noexcept {
    for (u32 i = 0; i < N; i++) {
        be_swap(val[i]);
    }
}

template<typename T>
constexpr void be_swap(T array[], const u32 size) noexcept {
    for (u32 i = 0; i < size; i++) {
        be_swap(array[i]);
    }
}

template<>
constexpr void be_swap(Mtx44& val) noexcept {
    for (auto & x : val) {
        for (float & y : x) {
            be_swap(y);
        }
    }
}

template<>
constexpr void be_swap(Mtx& val) noexcept {
    for (auto & x : val) {
        for (float & y : x) {
            be_swap(y);
        }
    }
}

#define LE(T) T
#define BE(T) BE<T>
#define BE_HOST(T) (T.host())
#else
#define BE(T) T
#define BE_HOST(T) (T)
#endif
