/* Copyright 2020 王一 Wang Yi <godspeed_china@yeah.net>
 * This is free and unencumbered software released into the public domain. http://unlicense.org/
 * See github.com/wangyi-fudan/wyhash/LICENSE
 */

#ifndef WYHASH_CLEAN_H
#define WYHASH_CLEAN_H

#include <stdint.h>
#include <string.h>
#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#pragma intrinsic(_umul128)
#endif

static inline uint64_t wy_mix(uint64_t a, uint64_t b)
{
#if defined(__SIZEOF_INT128__)
    __uint128_t r = (__uint128_t) a * b;
    return r ^ (r >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
    uint64_t rh, rl = _umul128(a, b, &rh);
    return rh ^ rl;
#else
    uint32_t ah = a >> 32, al = a;
    uint32_t bh = b >> 32, bl = b;
    uint64_t x = (uint64_t) al * bl, rl = (uint32_t) x;
    x = (uint64_t) ah * bl + (x >> 32);
    uint64_t y = (uint64_t) al * bh + (uint32_t) x;
    rl |= y << 32;
    uint64_t rh = (uint64_t) ah * bh + (x >> 32) + (y >> 32);
    return rh ^ rl;
#endif
}

static inline uint64_t wy_read8(const void *ptr)
{
    uint64_t res;
    memcpy(&res, ptr, sizeof(res));
    return res;
}

static inline uint32_t wy_read4(const void *ptr)
{
    uint32_t res;
    memcpy(&res, ptr, sizeof(res));
    return res;
}

static inline uint32_t wy_read3(const uint8_t *ptr, unsigned len)
{
    return ptr[0] << 16 | ptr[len >> 1] << 8 | ptr[len - 1];
}

#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#define WY_LIKELY(x) __builtin_expect(x, 1)
#else
#define WY_LIKELY(x) (x)
#endif

static inline uint64_t wyhash(const void *key, uint64_t len, uint64_t seed, const uint64_t *secret)
{
    uint64_t a, b;
    seed ^= secret[0];
    const uint8_t *ptr = (const uint8_t *) key;
    if (len > 16) {
        uint64_t seed2 = seed, seed3 = seed, n = len;
        while (n > 48) {
            seed  = wy_mix(wy_read8(ptr +  0) ^ secret[1], wy_read8(ptr +  8) ^ seed);
            seed2 = wy_mix(wy_read8(ptr + 16) ^ secret[2], wy_read8(ptr + 24) ^ seed2);
            seed3 = wy_mix(wy_read8(ptr + 32) ^ secret[3], wy_read8(ptr + 40) ^ seed3);
            ptr += 48;
            n -= 48;
        }
        seed ^= seed2 ^ seed3;
        while (n > 16) {
            seed = wy_mix(wy_read8(ptr) ^ secret[1], wy_read8(ptr + 8) ^ seed);
            ptr += 16;
            n -= 16;
        }
        a = wy_read8(ptr + n - 16);
        b = wy_read8(ptr + n -  8);
    } else if (WY_LIKELY(len >= 4)) {
        /*
        int64_t n = (int64_t) len - 8;
        uint64_t offs = n < 0 ? 0 : n;
        */
        //uint64_t offs = len > 8 ? 4 : 0;
        uint64_t offs = ((len - 1) >> 1) & 4;
        //uint64_t offs = (3 * len - 10) >> 3;
        //uint64_t offs = len < 8 ? 0 : len - 8;
        //uint64_t offs = len < 8 ? len - 4 : 4;
        a = wy_read4(ptr) | (uint64_t) wy_read4(ptr + offs) << 32;
        ptr += len - offs - 4;
        b = wy_read4(ptr) | (uint64_t) wy_read4(ptr + offs) << 32;
    } else {
        a = WY_LIKELY(len) ? wy_read3(ptr, len) : 0;
        b = 0;
    }
    //return wy_mix(secret[1] ^ len, wy_mix(a ^ secret[1], b ^ seed));
    return wy_mix(a ^ secret[1], b ^ seed ^ len);
}

static const uint64_t wy_secret[5] = {
    0xA0761D6478BD642Full, 0xE7037ED1A0B428DBull, 0x8EBC6AF09C88C6E3ull, 0x589965CC75374CC3ull, 0x1D8E4E27C47D124Full
};

#endif  // WYHASH_CLEAN_H
