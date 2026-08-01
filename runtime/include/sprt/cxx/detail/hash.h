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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_HASH_H_
#define RUNTIME_INCLUDE_SPRT_CXX_HASH_H_

#include <sprt/cxx/bit>
#include <sprt/cxx/__type_traits/modifications.h>
#include <sprt/runtime/hash.h>
#include <sprt/cxx/__type_traits/types.h>
#include <sprt/c/__sprt_math.h>

namespace sprt {

template <typename T>
struct hash;

template <unsigned_integer I>
struct hash<I> {
	constexpr size_t operator()(const I &i) const noexcept { return i & Max<size_t>; }
};

template <signed_integer I>
struct hash<I> {
	constexpr size_t operator()(const I &i) const noexcept {
		return sprt::bit_cast<sprt::make_unsigned_t<sprt::remove_cvref_t<decltype(i)>>>(i)
				& Max<size_t>;
	}
};

template <enumeration Enum>
struct hash<Enum> {
	constexpr size_t operator()(const Enum &i) const noexcept {
		return hash< sprt::underlying_type_t<Enum>>()(sprt::to_underlying(i));
	}
};

template <>
struct hash<float> {
	constexpr size_t operator()(const float &value) const noexcept {
		// equal keys must hash equal: -0.0f == 0.0f but their bit patterns differ
		return sprt::bit_cast<uint32_t>(value == 0.0f ? 0.0f : value) & Max<size_t>;
	}
};

template <>
struct hash<double> {
	constexpr size_t operator()(const double &value) const noexcept {
		// equal keys must hash equal: -0.0 == 0.0 but their bit patterns differ
		return sprt::bit_cast<uint64_t>(value == 0.0 ? 0.0 : value) & Max<size_t>;
	}
};

template <>
struct hash<long double> {
	size_t operator()(const long double &value) const noexcept {
		// equal keys must hash equal: -0.0L == 0.0L but their bit patterns differ
		const long double norm = (value == 0.0L) ? 0.0L : value;
		return sprt::hashSize((const char *)&norm, sizeof(norm));
	}
};

template <>
struct hash<char *> {
	size_t operator()(char *value) const noexcept {
		return sprt::hashSize(value, __constexpr_strlen(value));
	}
};

template <>
struct hash<const char *> {
	size_t operator()(const char *value) const noexcept {
		return sprt::hashSize(value, __constexpr_strlen(value));
	}
};

template <size_t N>
struct hash<const char (&)[N]> {
	// hash up to the first NUL (like hash<const char *>) so a string literal and
	// an equal const char* produce the same hash; hashing all N bytes would
	// include the terminator and break transparent lookups.
	size_t operator()(const char (&value)[N]) const noexcept {
		return sprt::hashSize(value, __constexpr_strlen(value));
	}
};

// The transparent hash<void> deduces T from `const T &` as the ARRAY type
// (char[N] / const char[N]), never as a reference-to-array - these are the
// specializations such lookups actually reach (same first-NUL contract).
template <size_t N>
struct hash<char[N]> {
	size_t operator()(const char (&value)[N]) const noexcept {
		return sprt::hashSize(value, __constexpr_strlen(value));
	}
};

template <size_t N>
struct hash<const char[N]> {
	size_t operator()(const char (&value)[N]) const noexcept {
		return sprt::hashSize(value, __constexpr_strlen(value));
	}
};

template <typename T>
struct hash<T *> {
	size_t operator()(const T *value) const noexcept {
		size_t __a = reinterpret_cast<size_t>(value);
		if constexpr (sizeof(size_t) == 8) {
			return __a * static_cast<size_t>(0x9E37'79B9'7F4A'7C15ull);
		} else {
			return __a * static_cast<size_t>(0x9E37'79B9u);
		}
	}
};

template <>
struct hash<void *> {
	size_t operator()(const void *value) const noexcept { return reinterpret_cast<size_t>(value); }
};

template <>
struct hash<void> {
	using is_transparent = void;

	template <typename T>
	constexpr size_t operator()(const T &value) const noexcept {
		return hash<T>()(value);
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX_HASH_H_
