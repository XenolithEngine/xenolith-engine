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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_ENUM_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_ENUM_H_

#include <sprt/runtime/math.h>
#include <sprt/cxx/iterator>
#include <sprt/cxx/bit>

namespace sprt {

/**
 * @file enum.h
 * @brief Enumeration utilities for iterating over enum values and flags
 * 
 * Public interfaces:
 * - toInt<E>(e)         Convert enum E to underlying integer type
 *                       Example: sprt::toInt(MyEnum::Value1)
 * 
 * - each<>()            Iterate over range of enum values using for-loop
 *                       Examples:
 *                         for (auto e : each<MyEnum>()) { } // 0 to Max-1
 *                         for (auto e : each<MyEnum, MyEnum::First, MyEnum::Last>()) { }
 * 
 * - flags<>()           Iterate over set bits in unsigned enum flags
 *                       Examples:
 *                         for (auto bit : flags(MyFlags::A | MyFlags::B)) { }
 *                         for (auto bit : flags<MyFlags::READ | MyFlags::WRITE>()) { }
 * 
 * - hasFlag(mask, flag) Check if any bits from flag are present in mask
 *                       Example: if (sprt::hasFlag(flags, MyFlags::READ)) { }
 * 
 * - hasFlagAll(mask, flag) Check if all bits from flag are set in mask
 *                         Example: if (sprt::hasFlagAll(flags, MyFlags::READ | MyFlags::WRITE)) { }
 * 
 * - SPRT_DEFINE_ENUM_AS_INCREMENTABLE(Type, First, Last)  Macro adding cyclic operator++/operator-- to enum
 *                       Usage: Define enum with First/Last values, then apply macro
 *                         enum class State { Init, Running, Stopped };
 *                         SPRT_DEFINE_ENUM_AS_INCREMENTABLE(State, State::Init, State::Stopped);
 * 
 * - SPRT_DEFINE_ENUM_AS_MASK(Type)                      Macro adding bitwise operators to enum as bitmask
 *                                                       Example:
 *                                                         enum class Flags : uint32_t { A = 1, B = 2 };
 *                                                         SPRT_DEFINE_ENUM_AS_MASK(Flags);
 *                                                         auto combined = Flags::A | Flags::B;
 */

namespace detail {

template <typename T>
struct ToIntWrapperType {
	static_assert(is_integral_v<T> or is_enum_v<T>);

	using type = decltype([]() {
		if constexpr (is_enum_v<T>) {
			return underlying_type_t<T>();
		} else {
			return T{};
		}
	}());
};

} // namespace detail

template <typename E>
constexpr typename detail::ToIntWrapperType<E>::type toInt(const E &e) {
	return static_cast<typename detail::ToIntWrapperType<E>::type>(e);
}

namespace detail {

template <typename E>
struct enum_iterator_end { };

template <typename E, E Last>
struct enum_iterator {
	using iterator_category = random_access_iterator_tag;
	using size_type = size_t;
	using pointer = E *;
	using reference = E;
	using difference_type = ptrdiff_t;
	using value_type = E;

	using iterator = enum_iterator<E, Last>;

	constexpr enum_iterator() noexcept : value(E(0)) { }
	constexpr enum_iterator(const enum_iterator &other) noexcept = default;

	explicit constexpr enum_iterator(E e) : value(toInt(e)) { }
	explicit constexpr enum_iterator(typename underlying_type<E>::type e) : value(e) { }

	constexpr iterator &operator=(const iterator &other) noexcept {
		value = other.value;
		return *this;
	}
	constexpr bool operator==(const iterator &other) const { return value == other.value; }
	constexpr bool operator!=(const iterator &other) const { return value != other.value; }
	constexpr bool operator<(const iterator &other) const { return value < other.value; }
	constexpr bool operator>(const iterator &other) const { return value > other.value; }
	constexpr bool operator<=(const iterator &other) const { return value <= other.value; }
	constexpr bool operator>=(const iterator &other) const { return value >= other.value; }

	constexpr bool operator==(const enum_iterator_end<E> &other) const {
		return value > toInt(Last);
	}
	constexpr bool operator!=(const enum_iterator_end<E> &other) const {
		return value <= toInt(Last);
	}

	constexpr iterator &operator++() {
		++value;
		return *this;
	}
	constexpr iterator operator++(int) {
		auto tmp = *this;
		++value;
		return tmp;
	}
	constexpr iterator &operator--() {
		--value;
		return *this;
	}
	constexpr iterator operator--(int) {
		auto tmp = *this;
		--value;
		return tmp;
	}
	constexpr iterator &operator+=(size_type n) {
		value += n;
		return *this;
	}
	constexpr iterator &operator-=(size_type n) {
		value -= n;
		return *this;
	}
	constexpr difference_type operator-(const iterator &other) const { return value - other.value; }

	constexpr reference operator*() const { return E(value); }
	constexpr pointer operator->() const { return &value; }
	constexpr reference operator[](size_type n) const { return *(value + n); }

	constexpr size_type operator-(pointer p) const { return value - p; }

	friend constexpr auto operator+(size_type n, const iterator &it) -> iterator {
		return iterator(it.value + n);
	}

	friend constexpr auto operator+(const iterator &it, size_type n) -> iterator {
		return iterator(it.value + n);
	}

	friend constexpr auto operator-(const iterator &it, size_type n) -> iterator {
		return iterator(it.value - n);
	}

	typename ToIntWrapperType<E>::type value;
};

template <typename E>
struct flags_iterator_end { };

template <typename E>
struct flags_iterator {
	using iterator_category = forward_iterator_tag;
	using size_type = size_t;
	using pointer = E *;
	using reference = E;
	using difference_type = ptrdiff_t;
	using value_type = E;
	using int_type = typename ToIntWrapperType<E>::type;

	using iterator = flags_iterator<E>;

	constexpr flags_iterator() noexcept : value(1) { }
	constexpr flags_iterator(int v, typename ToIntWrapperType<E>::type f) noexcept
	: value(v), flags(f) { }
	constexpr flags_iterator(const flags_iterator &other) noexcept = default;

	constexpr iterator &operator=(const iterator &other) noexcept {
		value = other.value;
		return *this;
	}
	constexpr bool operator==(const iterator &other) const { return value == other.value; }
	constexpr bool operator!=(const iterator &other) const { return value != other.value; }
	constexpr bool operator<(const iterator &other) const { return value < other.value; }
	constexpr bool operator>(const iterator &other) const { return value > other.value; }
	constexpr bool operator<=(const iterator &other) const { return value <= other.value; }
	constexpr bool operator>=(const iterator &other) const { return value >= other.value; }

	constexpr bool operator==(const flags_iterator_end<E> &other) const {
		int maxBits = sizeof(E) * 8 - sprt::countl_zero(flags);
		return value >= maxBits;
	}

	constexpr bool operator!=(const flags_iterator_end<E> &other) const {
		int maxBits = sizeof(E) * 8 - sprt::countl_zero(flags);
		return value < maxBits;
	}

	constexpr iterator &operator++() {
		int maxBits = sizeof(E) * 8 - sprt::countl_zero(flags);
		do { ++value; } while ((flags & (int_type(1) << value)) == 0 && value < maxBits);
		return *this;
	}
	constexpr iterator operator++(int) {
		int maxBits = sizeof(E) * 8 - sprt::countl_zero(flags);
		auto tmp = *this;
		do { ++value; } while ((flags & (int_type(1) << value)) == 0 && value < maxBits);
		return tmp;
	}

	constexpr difference_type operator-(const iterator &other) const { return value - other.value; }

	constexpr reference operator*() const { return E(int_type(1) << value); }

	int value;
	typename ToIntWrapperType<E>::type flags;
};

template <typename E, E Value>
struct flags_iterator_static {
	using iterator_category = forward_iterator_tag;
	using size_type = size_t;
	using pointer = E *;
	using reference = E;
	using difference_type = ptrdiff_t;
	using value_type = E;
	using int_type = typename ToIntWrapperType<E>::type;

	using iterator = flags_iterator_static<E, Value>;

	static constexpr int MaxBits = sizeof(E) * 8 - sprt::countl_zero(Value);

	constexpr flags_iterator_static() noexcept : value(1) { }
	constexpr flags_iterator_static(int v) noexcept : value(v) { }
	constexpr flags_iterator_static(const flags_iterator_static &other) noexcept = default;

	constexpr iterator &operator=(const iterator &other) noexcept {
		value = other.value;
		return *this;
	}
	constexpr bool operator==(const iterator &other) const { return value == other.value; }
	constexpr bool operator!=(const iterator &other) const { return value != other.value; }
	constexpr bool operator<(const iterator &other) const { return value < other.value; }
	constexpr bool operator>(const iterator &other) const { return value > other.value; }
	constexpr bool operator<=(const iterator &other) const { return value <= other.value; }
	constexpr bool operator>=(const iterator &other) const { return value >= other.value; }

	constexpr bool operator==(const flags_iterator_end<E> &other) const { return value >= MaxBits; }

	constexpr bool operator!=(const flags_iterator_end<E> &other) const { return value < MaxBits; }

	constexpr iterator &operator++() {
		do { ++value; } while ((Value & (int_type(1) << value)) == 0 && value < MaxBits);
		return *this;
	}
	constexpr iterator operator++(int) {
		auto tmp = *this;
		do { ++value; } while ((Value & (int_type(1) << value)) == 0 && value < MaxBits);
		return tmp;
	}

	constexpr difference_type operator-(const iterator &other) const { return value - other.value; }

	constexpr reference operator*() const { return E(int_type(1) << value); }

	int value;
};

template <typename E, E First, E Last>
struct enum_wrapper {
	constexpr enum_iterator<E, Last> begin() const { return enum_iterator<E, Last>(First); }
	constexpr enum_iterator_end<E> end() const { return enum_iterator_end<E>(); }
};

template <typename E>
struct flags_wrapper {
	constexpr flags_iterator<E> begin() const {
		return flags_iterator<E>(sprt::countr_zero(value), value);
	}
	constexpr flags_iterator_end<E> end() const { return flags_iterator_end<E>(); }

	flags_wrapper(E e) : value(toInt(e)) { }

	typename ToIntWrapperType<E>::type value;
};

template <typename E, E Value>
struct flags_wrapper_static {
	constexpr flags_iterator_static<E, Value> begin() const {
		return flags_iterator_static<E, Value>(sprt::countr_zero(Value));
	}
	constexpr flags_iterator_end<E> end() const { return flags_iterator_end<E>(); }
};

} // namespace detail

template <typename E, E First, E Last>
constexpr auto each() -> detail::enum_wrapper<E, First, Last> {
	return detail::enum_wrapper<E, First, Last>();
}

template <typename E>
constexpr auto each() -> detail::enum_wrapper<E, E(0), E(toInt(E::Max) - 1)> {
	return detail::enum_wrapper<E, E(0), E(toInt(E::Max) - 1)>();
}

template <typename E>
auto flags(E flags) -> detail::flags_wrapper<E> {
	static_assert(is_unsigned_v<typename detail::ToIntWrapperType<E>::type>,
			"Flags should be unsigned");
	return detail::flags_wrapper<E>(flags);
}

template <uint64_t Value>
auto flags() {
	return detail::flags_wrapper_static<uint64_t, Value>();
}

template <typename T>
inline bool hasFlag(T mask, T flag) {
	return (mask & flag) != T(0);
}

template <typename T>
inline bool hasFlagAll(T mask, T flag) {
	return (mask & flag) == T(flag);
}

} // namespace sprt

/** SP_DEFINE_ENUM_AS_INCREMENTABLE adds operator++/operator-- for enumerations */
#define SPRT_DEFINE_ENUM_AS_INCREMENTABLE(Type, First, Last) \
	SP_COVERAGE_TRIVIAL inline constexpr Type& operator++(Type& a) { \
		auto value = ::sprt::toInt(a); \
		if (value >= ::sprt::toInt(Type::Last)) { value = ::sprt::toInt(Type::First); } else { ++ value; } \
		::memcpy(&a, &value, sizeof(Type)); \
		return a; \
	} \
	SP_COVERAGE_TRIVIAL inline constexpr Type& operator--(Type& a) { \
		auto value = ::sprt::toInt(a); \
		if (value <= ::sprt::toInt(Type::First)) { value = ::sprt::toInt(Type::Last); } else { -- value; } \
		::memcpy(&a, &value, sizeof(Type)); \
		return a; \
	} \
	SP_COVERAGE_TRIVIAL inline constexpr Type operator++(Type& a, int) { \
		auto value = ::sprt::toInt(a); auto result = value; \
		if (value >= ::sprt::toInt(Type::Last)) { value = ::sprt::toInt(Type::First); } else { ++ value; } \
		::memcpy(&a, &value, sizeof(Type)); \
		return static_cast<Type>(result); \
	} \
	SP_COVERAGE_TRIVIAL inline constexpr Type operator--(Type& a, int) { \
		auto value = ::sprt::toInt(a); auto result = value; \
		if (value <= ::sprt::toInt(Type::First)) { value = ::sprt::toInt(Type::Last); } else { -- value; } \
		::memcpy(&a, &value, sizeof(Type)); \
		return static_cast<Type>(result); \
	} \
	SP_COVERAGE_TRIVIAL inline constexpr Type operator+(const Type &a, const typename std::underlying_type<Type>::type &b) { \
		return Type(::sprt::math::add_cyclic(::sprt::toInt(a), b, ::sprt::toInt(Type::First), ::sprt::toInt(Type::Last))); \
	} \
	SP_COVERAGE_TRIVIAL inline constexpr Type operator+=(Type &a, const typename std::underlying_type<Type>::type &b) { \
		auto value = ::sprt::math::add_cyclic(::sprt::toInt(a), b, ::sprt::toInt(Type::First), ::sprt::toInt(Type::Last)); \
		::memcpy(&a, &value, sizeof(Type)); \
		return a; \
	} \
	SP_COVERAGE_TRIVIAL inline constexpr Type operator-(const Type &a, const typename std::underlying_type<Type>::type &b) { \
		return Type(::sprt::math::sub_cyclic(::sprt::toInt(a), b, ::sprt::toInt(Type::First), ::sprt::toInt(Type::Last))); \
	} \
	SP_COVERAGE_TRIVIAL inline constexpr Type operator-=(Type &a, const typename std::underlying_type<Type>::type &b) { \
		auto value = ::sprt::math::sub_cyclic(::sprt::toInt(a), b, ::sprt::toInt(Type::First), ::sprt::toInt(Type::Last)); \
		::memcpy(&a, &value, sizeof(Type)); \
		return a; \
	}

/** SPRT_DEFINE_ENUM_AS_MASK is utility to make a bitwise-mask from typed enum
 * It defines a set of overloaded operators, that allow some bitwise operations
 * on this enum class
 *
 * Type should be unsigned, and SDK code style suggests to make it sized (uint32_t, uint64_t)
 */
#define SPRT_DEFINE_ENUM_AS_MASK(Type) \
	static_assert(::sprt::is_unsigned_v<::sprt::underlying_type_t<Type>>, #Type " should be unsigned");\
	SPRT_COVERAGE_TRIVIAL constexpr inline Type operator | (const Type &l, const Type &r) { return Type(::sprt::toInt(l) | ::sprt::toInt(r)); } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type operator & (const Type &l, const Type &r) { return Type(::sprt::toInt(l) & ::sprt::toInt(r)); } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type operator ^ (const Type &l, const Type &r) { return Type(::sprt::toInt(l) ^ ::sprt::toInt(r)); } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type & operator |= (Type &l, const Type &r) { l = Type(::sprt::toInt(l) | ::sprt::toInt(r)); return l; } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type & operator &= (Type &l, const Type &r) { l = Type(::sprt::toInt(l) & ::sprt::toInt(r)); return l; } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type & operator ^= (Type &l, const Type &r) { l = Type(::sprt::toInt(l) ^ ::sprt::toInt(r)); return l; } \
	SPRT_COVERAGE_TRIVIAL constexpr inline bool operator == (const Type &l, const ::sprt::underlying_type<Type>::type &r) { return ::sprt::toInt(l) == r; } \
	SPRT_COVERAGE_TRIVIAL constexpr inline bool operator == (const ::sprt::underlying_type<Type>::type &l, const Type &r) { return l == ::sprt::toInt(r); } \
	SPRT_COVERAGE_TRIVIAL constexpr inline bool operator != (const Type &l, const ::sprt::underlying_type<Type>::type &r) { return ::sprt::toInt(l) != r; } \
	SPRT_COVERAGE_TRIVIAL constexpr inline bool operator != (const ::sprt::underlying_type<Type>::type &l, const Type &r) { return l != ::sprt::toInt(r); } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type operator~(const Type &t) { return Type(~::sprt::toInt(t)); }

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_ENUM_H_
