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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___MEMORY_COMMON_H_
#define RUNTIME_INCLUDE_SPRT_CXX___MEMORY_COMMON_H_

#include <sprt/cxx/new>
#include <sprt/cxx/__utility/common.h>

#if __SPRT_USE_STL
#else

namespace std {
// support for a constexpr construct_at
template <typename _Tp, typename... _Args,
		typename = decltype(::new (sprt::declval<void *>(), sprt::nothrow)
						_Tp(sprt::declval<_Args>()...))>
constexpr _Tp *construct_at(_Tp *__location, _Args &&...__args) {
	return ::new (static_cast<void *>(__location)) _Tp(sprt::forward<_Args>(__args)...);
}

} // namespace std

#endif

namespace sprt {

template <typename _Tp, typename... _Args>
constexpr _Tp *__construct_at(_Tp *__location, _Args &&...__args) {
	return ::new (static_cast<void *>(__location), sprt::nothrow)
			_Tp(sprt::forward<_Args>(__args)...);
}

template <typename _Tp, typename... _Args,
		typename = decltype(::new (sprt::declval<void *>(), sprt::nothrow)
						_Tp(sprt::declval<_Args>()...))>
constexpr _Tp *construct_at(_Tp *__location, _Args &&...__args) {
	if (__builtin_is_constant_evaluated()) {
		return std::construct_at(__location, sprt::forward<_Args>(__args)...);
	} else {
		return __construct_at(__location, sprt::forward<_Args>(__args)...);
	}
}

template <typename _Tp>
requires (!is_array_v<_Tp>)
constexpr void destroy_at(_Tp *__loc) {
	__loc->~_Tp();
}

template <typename _Tp>
requires (is_array_v<_Tp>)
constexpr void destroy_at(_Tp *__loc) {
	for (auto &&__val : *__loc) { sprt::destroy_at(sprt::addressof(__val)); }
}

template <typename _ForwardIterator>
constexpr void destroy(_ForwardIterator __first, _ForwardIterator __last) {
	for (; __first != __last; ++__first) { sprt::destroy_at(sprt::addressof(*__first)); }
}

template <typename _ForwardIterator, typename _Size>
constexpr _ForwardIterator destroy_n(_ForwardIterator __first, _Size __n) {
	for (; __n > 0; (void)++__first, --__n) { sprt::destroy_at(sprt::addressof(*__first)); }
	return __first;
}

} // namespace sprt

#endif
