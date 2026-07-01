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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_SFINAE_HELPERS_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_SFINAE_HELPERS_H_

// Shared SFINAE base classes, ported from LLVM libc++ <__tuple/sfinae_helpers.h>.
// optional<T> and variant<Types...> mix these in to make their copy/move
// constructor and assignment conditionally available (and to disable the
// conditionally-explicit constructors) based on the properties of T / Types.

namespace sprt {

// When selected, disables every conditionally-explicit constructor / assignment
// of a wrapper that mixes it in (libc++ __check_tuple_constructor_fail).
struct __check_tuple_constructor_fail {
	template <typename...>
	static constexpr bool __enable_implicit() {
		return false;
	}
	template <typename...>
	static constexpr bool __enable_explicit() {
		return false;
	}
	template <typename...>
	static constexpr bool __enable_assign() {
		return false;
	}
};

// __sfinae_ctor_base / __sfinae_assign_base: empty mixins whose copy/move special
// members are conditionally deleted, so a derived wrapper only offers a copy/move
// constructor (resp. assignment) when its element(s) are copy/move constructible
// (resp. assignable).
template <bool _CanCopy, bool _CanMove>
struct __sfinae_ctor_base { };

template <>
struct __sfinae_ctor_base<false, false> {
	__sfinae_ctor_base() = default;
	__sfinae_ctor_base(const __sfinae_ctor_base &) = delete;
	__sfinae_ctor_base(__sfinae_ctor_base &&) = delete;
	__sfinae_ctor_base &operator=(const __sfinae_ctor_base &) = default;
	__sfinae_ctor_base &operator=(__sfinae_ctor_base &&) = default;
};

template <>
struct __sfinae_ctor_base<true, false> {
	__sfinae_ctor_base() = default;
	__sfinae_ctor_base(const __sfinae_ctor_base &) = default;
	__sfinae_ctor_base(__sfinae_ctor_base &&) = delete;
	__sfinae_ctor_base &operator=(const __sfinae_ctor_base &) = default;
	__sfinae_ctor_base &operator=(__sfinae_ctor_base &&) = default;
};

template <>
struct __sfinae_ctor_base<false, true> {
	__sfinae_ctor_base() = default;
	__sfinae_ctor_base(const __sfinae_ctor_base &) = delete;
	__sfinae_ctor_base(__sfinae_ctor_base &&) = default;
	__sfinae_ctor_base &operator=(const __sfinae_ctor_base &) = default;
	__sfinae_ctor_base &operator=(__sfinae_ctor_base &&) = default;
};

template <bool _CanCopy, bool _CanMove>
struct __sfinae_assign_base { };

template <>
struct __sfinae_assign_base<false, false> {
	__sfinae_assign_base() = default;
	__sfinae_assign_base(const __sfinae_assign_base &) = default;
	__sfinae_assign_base(__sfinae_assign_base &&) = default;
	__sfinae_assign_base &operator=(const __sfinae_assign_base &) = delete;
	__sfinae_assign_base &operator=(__sfinae_assign_base &&) = delete;
};

template <>
struct __sfinae_assign_base<true, false> {
	__sfinae_assign_base() = default;
	__sfinae_assign_base(const __sfinae_assign_base &) = default;
	__sfinae_assign_base(__sfinae_assign_base &&) = default;
	__sfinae_assign_base &operator=(const __sfinae_assign_base &) = default;
	__sfinae_assign_base &operator=(__sfinae_assign_base &&) = delete;
};

template <>
struct __sfinae_assign_base<false, true> {
	__sfinae_assign_base() = default;
	__sfinae_assign_base(const __sfinae_assign_base &) = default;
	__sfinae_assign_base(__sfinae_assign_base &&) = default;
	__sfinae_assign_base &operator=(const __sfinae_assign_base &) = delete;
	__sfinae_assign_base &operator=(__sfinae_assign_base &&) = default;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_SFINAE_HELPERS_H_
