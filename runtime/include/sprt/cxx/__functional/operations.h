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

// [arithmetic.operations] / [comparisons] / [logical.operations] / [bitwise.operations]
// arithmetic, logical and bitwise function objects (the comparison ones — less/greater/... —
// live in __functional/compare.h). Each has the transparent void specialization.

#ifndef RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_OPERATIONS_H_
#define RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_OPERATIONS_H_

#include <sprt/cxx/__utility/common.h>

namespace sprt {

// value-returning binary (result type _Tp for the homogeneous form)
#define __SPRT_BINARY_OP(_Name, _Op) \
	template <typename _Tp = void> \
	struct _Name { \
		constexpr _Tp operator()(const _Tp &__x, const _Tp &__y) const { return __x _Op __y; } \
	}; \
	template <> \
	struct _Name<void> { \
		template <typename _T1, typename _T2> \
		constexpr auto operator()(_T1 &&__x, _T2 &&__y) const \
				-> decltype(sprt::forward<_T1>(__x) _Op sprt::forward<_T2>(__y)) { \
			return sprt::forward<_T1>(__x) _Op sprt::forward<_T2>(__y); \
		} \
		using is_transparent = void; \
	}

// bool-returning binary (logical)
#define __SPRT_LOGICAL_OP(_Name, _Op) \
	template <typename _Tp = void> \
	struct _Name { \
		constexpr bool operator()(const _Tp &__x, const _Tp &__y) const { return __x _Op __y; } \
	}; \
	template <> \
	struct _Name<void> { \
		template <typename _T1, typename _T2> \
		constexpr auto operator()(_T1 &&__x, _T2 &&__y) const \
				-> decltype(sprt::forward<_T1>(__x) _Op sprt::forward<_T2>(__y)) { \
			return sprt::forward<_T1>(__x) _Op sprt::forward<_T2>(__y); \
		} \
		using is_transparent = void; \
	}

// unary
#define __SPRT_UNARY_OP(_Name, _Op, _Ret) \
	template <typename _Tp = void> \
	struct _Name { \
		constexpr _Ret operator()(const _Tp &__x) const { return _Op __x; } \
	}; \
	template <> \
	struct _Name<void> { \
		template <typename _T1> \
		constexpr auto operator()(_T1 &&__x) const -> decltype(_Op sprt::forward<_T1>(__x)) { \
			return _Op sprt::forward<_T1>(__x); \
		} \
		using is_transparent = void; \
	}

__SPRT_BINARY_OP(plus, +);
__SPRT_BINARY_OP(minus, -);
__SPRT_BINARY_OP(multiplies, *);
__SPRT_BINARY_OP(divides, /);
__SPRT_BINARY_OP(modulus, %);
__SPRT_UNARY_OP(negate, -, _Tp);

__SPRT_LOGICAL_OP(logical_and, &&);
__SPRT_LOGICAL_OP(logical_or, ||);
__SPRT_UNARY_OP(logical_not, !, bool);

__SPRT_BINARY_OP(bit_and, &);
__SPRT_BINARY_OP(bit_or, |);
__SPRT_BINARY_OP(bit_xor, ^);
__SPRT_UNARY_OP(bit_not, ~, _Tp);

#undef __SPRT_BINARY_OP
#undef __SPRT_LOGICAL_OP
#undef __SPRT_UNARY_OP

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_OPERATIONS_H_
