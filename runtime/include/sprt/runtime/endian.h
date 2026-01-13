/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_ENDINAN_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_ENDINAN_H_

#include <sprt/runtime/int.h>
#include <sprt/runtime/bit_cast.h>

namespace sprt::byteorder {

#if __SPRT_HAS_BUILTIN(__builtin_bswap16)
SPRT_FORCEINLINE uint16_t bswap16(uint16_t x) { return __builtin_bswap16(x); }
#else
SPRT_FORCEINLINE uint16_t bswap16(uint16_t x) { return __builtin_bswap32(x) << 16; }
#endif

#if __SPRT_HAS_BUILTIN(__builtin_bswap32)
SPRT_FORCEINLINE uint32_t bswap32(uint32_t x) { return __builtin_bswap32(x); }
#else
SPRT_FORCEINLINE uint32_t bswap32(uint32_t x) {
	return x & 0xFF << 24 | (x >> 8 & 0xFF) << 16 | (x >> 16 & 0xFF) << 8 | (x >> 24 & 0xFF);
}
#endif

#if __SPRT_HAS_BUILTIN(__builtin_bswap64)
SPRT_FORCEINLINE uint64_t bswap64(uint64_t x) { return __builtin_bswap64(x); }
#else
SPRT_FORCEINLINE uint64_t bswap64(uint64_t x) {
	return x & 0xFF << 56 | (x >> 8 & 0xFF) << 48 | (x >> 16 & 0xFF) << 40 | (x >> 24 & 0xFF) << 32
			| (x >> 32 & 0xFF) << 24 | (x >> 40 & 0xFF) << 16 | (x >> 48 & 0xFF) << 8
			| (x >> 56 & 0xFF);
}
#endif

} // namespace sprt::byteorder


namespace sprt {

/*
	endian
*/


#ifndef __BYTE_ORDER__
#error "__BYTE_ORDER__ is not defined"
#endif

enum class endian {
	little = 0xDEAD,
	big = 0xFACE,
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	native = little,
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	native = big,
#else
	native = 0xCAFE,
#endif
	network = big,
};

} // namespace sprt

namespace sprt::byteorder {

enum class ShouldSwap {
	NoSwap,
	DoSwap,
	CustomSwap,
};

static constexpr size_t Bit8Size = 1;
static constexpr size_t Bit16Size = 2;
static constexpr size_t Bit32Size = 4;
static constexpr size_t Bit64Size = 8;

template <class T, ShouldSwap ByteSwap, size_t Size>
struct Converter;

template <class T>
struct Converter<T, ShouldSwap::DoSwap, Bit8Size> {
	static inline T Swap(T value) { return value; }
};

template <class T>
struct Converter<T, ShouldSwap::DoSwap, Bit16Size> {
	static inline T Swap(T value) {
		return sprt::bit_cast<T>(bswap16(sprt::bit_cast<uint16_t>(value)));
	}
};

template <class T>
struct Converter<T, ShouldSwap::DoSwap, Bit32Size> {
	static inline T Swap(T value) {
		return sprt::bit_cast<T>(bswap32(sprt::bit_cast<uint32_t>(value)));
	}
};

template <class T>
struct Converter<T, ShouldSwap::DoSwap, Bit64Size> {
	static inline T Swap(T value) {
		return sprt::bit_cast<T>(bswap64(sprt::bit_cast<uint64_t>(value)));
	}
};

template <class T, size_t Size>
struct Converter<T, ShouldSwap::DoSwap, Size> {
	static inline T Swap(T value) {
		T ret;
		char *source = (char *)&source;
		auto end = source + Size;

		char *target = ((char *)&ret) + Size - 1;
		while (source != end) { *target-- = *source++; }
		return ret;
	}
};

template <class T, size_t Size>
struct Converter<T, ShouldSwap::NoSwap, Size> {
	static inline T Swap(T value) { return value; }
};


static constexpr ShouldSwap hostToNetwork() {
	if constexpr (endian::native == endian::network) {
		return ShouldSwap::NoSwap;
	}
	if constexpr (endian::native == endian::little) {
		return ShouldSwap::DoSwap;
	}
	return ShouldSwap::CustomSwap;
}

static constexpr ShouldSwap hostToLittle() {
	if constexpr (endian::native == endian::little) {
		return ShouldSwap::NoSwap;
	}
	if constexpr (endian::native == endian::big) {
		return ShouldSwap::DoSwap;
	}
	return ShouldSwap::CustomSwap;
}

static constexpr ShouldSwap hostToBig() {
	if constexpr (endian::native == endian::big) {
		return ShouldSwap::NoSwap;
	}
	if constexpr (endian::native == endian::little) {
		return ShouldSwap::DoSwap;
	}
	return ShouldSwap::CustomSwap;
}

static constexpr bool isLittleEndian() { return endian::native == endian::little; }

template <class T>
using NetworkConverter = Converter<T, hostToNetwork(), sizeof(T)>;

template <class T>
using LittleConverter = Converter<T, hostToLittle(), sizeof(T)>;

template <class T>
using BigConverter = Converter<T, hostToBig(), sizeof(T)>;

template <class T>
using HostConverter = Converter<T, ShouldSwap::NoSwap, sizeof(T)>;

template <class T>
static inline T HostToNetwork(T value) {
	return NetworkConverter<T>::Swap(value);
}

template <class T>
static inline T NetworkToHost(T value) {
	return NetworkConverter<T>::Swap(value);
}

template <class T>
static inline T HostToLittle(T value) {
	return LittleConverter<T>::Swap(value);
}

template <class T>
static inline T LittleToHost(T value) {
	return LittleConverter<T>::Swap(value);
}

template <class T>
static inline T HostToBig(T value) {
	return BigConverter<T>::Swap(value);
}

template <class T>
static inline T BigToHost(T value) {
	return BigConverter<T>::Swap(value);
}

template <endian Endianess, typename T>
struct ConverterTraits;

template <typename T>
struct ConverterTraits<endian::big, T> : BigConverter<T> { };

template <typename T>
struct ConverterTraits<endian::little, T> : LittleConverter<T> { };

} // namespace sprt::byteorder

#endif
