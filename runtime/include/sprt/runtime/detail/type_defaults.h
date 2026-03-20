/**
 Copyright (c) 2026 Xenolith Team Team <admin@xenolith.studio>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_DEFAULTS_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_DEFAULTS_H_

#define __SPRT_STL_BOOL_BUILTIN(Name) \
	template <typename Type> \
	struct Name { \
		static constexpr bool value = __##Name(Type);\
	}; \
	template <typename Type> \
	inline constexpr bool Name##_v =  __##Name(Type);

namespace sprt {

using nullptr_t = decltype(nullptr);

template <typename...>
using void_t = void;

struct __nat {
	__nat() = delete;
	__nat(const __nat &) = delete;
	__nat &operator=(const __nat &) = delete;
	~__nat() = delete;
};


template <typename Type, Type __v>
struct integral_constant {
	static inline constexpr const Type value = __v;
	typedef Type value_type;
	typedef integral_constant type;
	constexpr operator value_type() const noexcept { return value; }
	constexpr value_type operator()() const noexcept { return value; }
};

typedef integral_constant<bool, true> true_type;
typedef integral_constant<bool, false> false_type;

template <bool __b>
using bool_constant = integral_constant<bool, __b>;


struct __empty { };

template <typename Type>
struct type_identity {
	typedef Type type;
};

template <typename Type>
using type_identity_t = typename type_identity<Type>::type;

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_DEFAULTS_H_
