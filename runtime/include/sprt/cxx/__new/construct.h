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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___NEW_CONSTRUCT_H_
#define RUNTIME_INCLUDE_SPRT_CXX___NEW_CONSTRUCT_H_

#include <sprt/c/__sprt_stddef.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/__new/nothrow.h>

#if !__SPRT_USE_STL

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
constexpr _Tp *__construct_at(_Tp *__location, _Args &&...__args) noexcept {
	return ::new (static_cast<void *>(__location), sprt::nothrow)
			_Tp(sprt::forward<_Args>(__args)...);
}

template <typename _Tp, typename... _Args,
		typename = decltype(::new (sprt::declval<void *>(), sprt::nothrow)
						_Tp(sprt::declval<_Args>()...))>
constexpr _Tp *construct_at(_Tp *__location, _Args &&...__args) noexcept {
	if (__builtin_is_constant_evaluated()) {
		return std::construct_at(__location, sprt::forward<_Args>(__args)...);
	} else {
		return __construct_at(__location, sprt::forward<_Args>(__args)...);
	}
}

template <typename _Tp>
requires (!is_array_v<_Tp>)
constexpr void destroy_at(_Tp *__loc) noexcept {
	__loc->~_Tp();
}

template <typename _Tp>
requires (is_array_v<_Tp>)
constexpr void destroy_at(_Tp *__loc) noexcept {
	for (auto &&__val : *__loc) { sprt::destroy_at(sprt::addressof(__val)); }
}

template <typename T>
inline void __delete(T *t) noexcept {
	sprt::destroy_at(t);
	// pool-owned storage (sprt::detail::AllocPool subclasses expose __sprt_pool_owned_tag):
	// the destructor ran above, but the block belongs to a memory pool that reclaims it -
	// ::free() on a pool pointer is undefined. <new> can't include allocator_pool.h (cycle),
	// so the marker is detected structurally rather than via is_base_of.
	if constexpr (requires { typename T::__sprt_pool_owned_tag; }) {
		return;
	} else if constexpr (alignof(T) <= alignof(__sprt_max_align_t)) {
		__sprt_free(static_cast<void *>(t));
	} else {
		__sprt_aligned_free(static_cast<void *>(t));
	}
}

template <typename T>
requires (sprt::is_same_v<T, char> || sprt::is_same_v<T, unsigned char>)
inline void __delete(const T *t) noexcept {
	__delete(const_cast<T *>(t));
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___NEW_CONSTRUCT_H_
