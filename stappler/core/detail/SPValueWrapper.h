/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_CORE_DETAIL_SPVALUEWRAPPER_H_
#define CORE_CORE_DETAIL_SPVALUEWRAPPER_H_

#include "SPMath.h"

// A part of SPCore.h, DO NOT include this directly

namespace STAPPLER_VERSIONIZED stappler {

/** Value wrapper is a syntactic sugar struct, that allow you to create
 * an alias type for some other type, that will be statically and uniquely
 * different from all other types
 *
 * Most common usage is type-based overload resolution, like
 *
 * using FilePath = ValueWrapper<StringView, class FilePathFlag>;
 * using DataString =  ValueWrapper<StringView, class DataStringFlag>;
 *
 * ...
 *
 * class SomeClass {
 * 	SomeClass(FilePath); // init with data from file
 * 	SomeClass(DataString); // init with data from memory
 * };
 *
 * Also, ValueWrapper used in named arguments implementation
 * and functions, that requires additional type-checking
 */

template <class T, class Flag>
struct ValueWrapper {
	using Type = T;

	static constexpr ValueWrapper<T, Flag> max() {
		return ValueWrapper<T, Flag>(NumericLimits<T>::max());
	}
	static constexpr ValueWrapper<T, Flag> min() {
		return ValueWrapper<T, Flag>(NumericLimits<T>::min());
	}
	static constexpr ValueWrapper<T, Flag> epsilon() {
		return ValueWrapper<T, Flag>(NumericLimits<T>::epsilon());
	}
	static constexpr ValueWrapper<T, Flag> zero() { return ValueWrapper<T, Flag>(0); }

	inline constexpr ValueWrapper() noexcept = default;
	inline explicit constexpr ValueWrapper(const T &val) noexcept : value(val) { }
	inline explicit constexpr ValueWrapper(T &&val) noexcept : value(std::move(val)) { }

	inline ValueWrapper(const ValueWrapper<T, Flag> &other) noexcept = default;
	inline ValueWrapper<T, Flag> &operator=(const ValueWrapper<T, Flag> &other) noexcept = default;

	inline ValueWrapper(ValueWrapper<T, Flag> &&other) noexcept = default;
	inline ValueWrapper<T, Flag> &operator=(ValueWrapper<T, Flag> &&other) noexcept = default;

	inline void set(const T &val) { value = val; }
	inline void set(T &&val) { value = std::move(val); }
	inline constexpr T &get() { return value; }
	inline constexpr const T &get() const { return value; }
	inline constexpr bool empty() const { return value == 0; }

	inline constexpr bool operator==(const ValueWrapper<T, Flag> &other) const {
		return value == other.value;
	}
	inline constexpr bool operator!=(const ValueWrapper<T, Flag> &other) const {
		return value != other.value;
	}
	inline constexpr bool operator>(const ValueWrapper<T, Flag> &other) const {
		return value > other.value;
	}
	inline constexpr bool operator<(const ValueWrapper<T, Flag> &other) const {
		return value < other.value;
	}
	inline constexpr bool operator>=(const ValueWrapper<T, Flag> &other) const {
		return value >= other.value;
	}
	inline constexpr bool operator<=(const ValueWrapper<T, Flag> &other) const {
		return value <= other.value;
	}

	inline constexpr void operator|=(const ValueWrapper<T, Flag> &other) { value |= other.value; }
	inline constexpr void operator&=(const ValueWrapper<T, Flag> &other) { value &= other.value; }
	inline constexpr void operator^=(const ValueWrapper<T, Flag> &other) { value ^= other.value; }
	inline constexpr void operator+=(const ValueWrapper<T, Flag> &other) { value += other.value; }
	inline constexpr void operator-=(const ValueWrapper<T, Flag> &other) { value -= other.value; }
	inline constexpr void operator*=(const ValueWrapper<T, Flag> &other) { value *= other.value; }
	inline constexpr void operator/=(const ValueWrapper<T, Flag> &other) { value /= other.value; }

	inline constexpr ValueWrapper<T, Flag> operator|(const ValueWrapper<T, Flag> &v) const {
		return ValueWrapper<T, Flag>(value | v.value);
	}
	inline constexpr ValueWrapper<T, Flag> operator&(const ValueWrapper<T, Flag> &v) const {
		return ValueWrapper<T, Flag>(value & v.value);
	}
	inline constexpr ValueWrapper<T, Flag> operator^(const ValueWrapper<T, Flag> &v) const {
		return ValueWrapper<T, Flag>(value ^ v.value);
	}
	inline constexpr ValueWrapper<T, Flag> operator+(const ValueWrapper<T, Flag> &v) const {
		return ValueWrapper<T, Flag>(value + v.value);
	}
	inline constexpr ValueWrapper<T, Flag> operator-(const ValueWrapper<T, Flag> &v) const {
		return ValueWrapper<T, Flag>(value - v.value);
	}
	inline constexpr ValueWrapper<T, Flag> operator*(const ValueWrapper<T, Flag> &v) const {
		return ValueWrapper<T, Flag>(value * v.value);
	}
	inline constexpr ValueWrapper<T, Flag> operator/(const ValueWrapper<T, Flag> &v) const {
		return ValueWrapper<T, Flag>(value / v.value);
	}
	inline constexpr ValueWrapper<T, Flag> operator-() const {
		return ValueWrapper<T, Flag>(-value);
	}

	inline ValueWrapper<T, Flag> &operator++() {
		value++;
		return *this;
	}
	inline ValueWrapper<T, Flag> &operator--() {
		value--;
		return *this;
	}

	inline ValueWrapper<T, Flag> operator++(int) {
		ValueWrapper<T, Flag> result(*this);
		++(*this);
		return result;
	}
	inline ValueWrapper<T, Flag> operator--(int) {
		ValueWrapper<T, Flag> result(*this);
		--(*this);
		return result;
	}

	// to enable progress
	template <typename M>
	inline constexpr std::enable_if_t<HasMultiplication<Type, M>::type::value,
			ValueWrapper<T, Flag>>
	operator*(const M &v) const {
		return ValueWrapper<T, Flag>(value * v);
	}

	auto operator<=>(const ValueWrapper &) const = default;

	T value;
};

} // namespace STAPPLER_VERSIONIZED stappler

namespace std {

template <typename Value, typename Flag>
struct hash<STAPPLER_VERSIONIZED_NAMESPACE::ValueWrapper<Value, Flag>> {
	hash() { }

	size_t operator()(
			const STAPPLER_VERSIONIZED_NAMESPACE::ValueWrapper<Value, Flag> &value) const noexcept {
		return hash<Value>()(value.get());
	}
};

} // namespace std

#endif /* CORE_CORE_DETAIL_SPVALUEWRAPPER_H_ */
