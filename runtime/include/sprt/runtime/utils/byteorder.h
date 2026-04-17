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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_UTILS_BYTEORDER_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_UTILS_BYTEORDER_H_

#include <sprt/runtime/init.h>
#include <sprt/cxx/bit>

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

template <typename T, ShouldSwap ByteSwap, size_t Size>
struct Converter;

template <typename T>
struct Converter<T, ShouldSwap::DoSwap, Bit8Size> {
	static inline T Swap(T value) { return value; }
};

template <typename T>
struct Converter<T, ShouldSwap::DoSwap, Bit16Size> {
	static inline T Swap(T value) {
		return sprt::bit_cast<T>(byteswap(sprt::bit_cast<uint16_t>(value)));
	}
};

template <typename T>
struct Converter<T, ShouldSwap::DoSwap, Bit32Size> {
	static inline T Swap(T value) {
		return sprt::bit_cast<T>(byteswap(sprt::bit_cast<uint32_t>(value)));
	}
};

template <typename T>
struct Converter<T, ShouldSwap::DoSwap, Bit64Size> {
	static inline T Swap(T value) {
		return sprt::bit_cast<T>(byteswap(sprt::bit_cast<uint64_t>(value)));
	}
};

template <typename T, size_t Size>
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

template <typename T, size_t Size>
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

template <typename T>
using NetworkConverter = Converter<T, hostToNetwork(), sizeof(T)>;

template <typename T>
using LittleConverter = Converter<T, hostToLittle(), sizeof(T)>;

template <typename T>
using BigConverter = Converter<T, hostToBig(), sizeof(T)>;

template <typename T>
using HostConverter = Converter<T, ShouldSwap::NoSwap, sizeof(T)>;

template <typename T>
static inline T HostToNetwork(T value) {
	return NetworkConverter<T>::Swap(value);
}

template <typename T>
static inline T NetworkToHost(T value) {
	return NetworkConverter<T>::Swap(value);
}

template <typename T>
static inline T HostToLittle(T value) {
	return LittleConverter<T>::Swap(value);
}

template <typename T>
static inline T LittleToHost(T value) {
	return LittleConverter<T>::Swap(value);
}

template <typename T>
static inline T HostToBig(T value) {
	return BigConverter<T>::Swap(value);
}

template <typename T>
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

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_UTILS_BYTEORDER_H_
