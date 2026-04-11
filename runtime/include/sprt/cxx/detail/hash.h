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

template <>
struct hash<float> {
	constexpr size_t operator()(const float &value) const noexcept {
		return sprt::bit_cast<uint32_t>(value) & Max<size_t>;
	}
};

template <>
struct hash<double> {
	constexpr size_t operator()(const double &value) const noexcept {
		return sprt::bit_cast<uint64_t>(value) & Max<size_t>;
	}
};

template <>
struct hash<long double> {
	size_t operator()(const double &value) const noexcept {
		return sprt::hashSize((const char *)&value, sizeof(value));
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
	size_t operator()(const char (&value)[N]) const noexcept { return sprt::hashSize(value, N); }
};

template <typename T>
struct hash<T *> {
	constexpr size_t operator()(const T *value) const noexcept {
		return static_cast<size_t>(sprt::rotr(reinterpret_cast<uintptr_t>(value),
				sprt::countr_zero(alignof(max_align_t))));
	}
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
