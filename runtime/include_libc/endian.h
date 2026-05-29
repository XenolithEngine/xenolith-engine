/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef CORE_RUNTIME_INCLUDE_LIBC_ENDIAN_H_
#define CORE_RUNTIME_INCLUDE_LIBC_ENDIAN_H_

#include <sprt/c/bits/__sprt_uint16_t.h>
#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>

#ifndef __BYTE_ORDER__
#error "__BYTE_ORDER__ is not defined"
#endif

// clang-format off
#ifndef __ORDER_POP_ENDIAN__
#define __ORDER_POP_ENDIAN__ 3412
#endif
// clang-format on

#define BIG_ENDIAN __ORDER_BIG_ENDIAN__
#define LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define PDP_ENDIAN __ORDER_POP_ENDIAN__
#define BYTE_ORDER __BYTE_ORDER__

SPRT_FORCEINLINE __SPRT_ID(uint16_t) __bswap16(__SPRT_ID(uint16_t) __x) {
	return __builtin_bswap16(__x);
}

SPRT_FORCEINLINE __SPRT_ID(uint32_t) __bswap32(__SPRT_ID(uint32_t) __x) {
	return __builtin_bswap32(__x);
}

SPRT_FORCEINLINE __SPRT_ID(uint64_t) __bswap64(__SPRT_ID(uint64_t) __x) {
	return __builtin_bswap64(__x);
}

#if defined(__SIZEOF_INT128__)
SPRT_FORCEINLINE unsigned __int128 __bswap128(unsigned __int128 __x) {
#if __has_builtin(__builtin_bswap128)
	return __builtin_bswap128(__x);
#else
	return (unsigned __int128)(__bswap64((__SPRT_ID(uint64_t))(__x))) << 64
			| (unsigned __int128)(__bswap64((__SPRT_ID(uint64_t))(__x >> 64)));
#endif // __has_builtin(__builtin_bswap128)
}
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define htobe16(x) __bswap16(x)
#define be16toh(x) __bswap16(x)
#define htobe32(x) __bswap32(x)
#define be32toh(x) __bswap32(x)
#define htobe64(x) __bswap64(x)
#define be64toh(x) __bswap64(x)
#define htole16(x) (__SPRT_ID(uint16_t))(x)
#define le16toh(x) (__SPRT_ID(uint16_t))(x)
#define htole32(x) (__SPRT_ID(uint32_t))(x)
#define le32toh(x) (__SPRT_ID(uint32_t))(x)
#define htole64(x) (__SPRT_ID(uint64_t))(x)
#define le64toh(x) (__SPRT_ID(uint64_t))(x)
#else
#define htobe16(x) (__SPRT_ID(uint16_t))(x)
#define be16toh(x) (__SPRT_ID(uint16_t))(x)
#define htobe32(x) (__SPRT_ID(uint32_t))(x)
#define be32toh(x) (__SPRT_ID(uint32_t))(x)
#define htobe64(x) (__SPRT_ID(uint64_t))(x)
#define be64toh(x) (__SPRT_ID(uint64_t))(x)
#define htole16(x) __bswap16(x)
#define le16toh(x) __bswap16(x)
#define htole32(x) __bswap32(x)
#define le32toh(x) __bswap32(x)
#define htole64(x) __bswap64(x)
#define le64toh(x) __bswap64(x)
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define betoh16(x) __bswap16(x)
#define betoh32(x) __bswap32(x)
#define betoh64(x) __bswap64(x)
#define letoh16(x) (__SPRT_ID(uint16_t))(x)
#define letoh32(x) (__SPRT_ID(uint32_t))(x)
#define letoh64(x) (__SPRT_ID(uint64_t))(x)
#else
#define betoh16(x) (__SPRT_ID(uint16_t))(x)
#define betoh32(x) (__SPRT_ID(uint32_t))(x)
#define betoh64(x) (__SPRT_ID(uint64_t))(x)
#define letoh16(x) __bswap16(x)
#define letoh32(x) __bswap32(x)
#define letoh64(x) __bswap64(x)
#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_ENDIAN_H_
