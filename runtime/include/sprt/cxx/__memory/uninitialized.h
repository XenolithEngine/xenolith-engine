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

// [specialized.algorithms] uninitialized memory algorithms + [ptr.align] std::align.
// This is -fno-exceptions, so the standard's "destroy on exception" rollback is a no-op
// and omitted (element construction never throws here). Element construction uses a
// placement-new (default-init for *_default_construct, value-init for *_value_construct)
// rather than construct_at, which always value-initializes.

#ifndef RUNTIME_INCLUDE_SPRT_CXX___MEMORY_UNINITIALIZED_H_
#define RUNTIME_INCLUDE_SPRT_CXX___MEMORY_UNINITIALIZED_H_

#include <sprt/cxx/cstddef>
#include <sprt/cxx/new>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/__utility/pair.h>
#include <sprt/cxx/__iterator/iterator_ops.h>

namespace sprt {

template <typename _FwdIt>
inline _FwdIt uninitialized_default_construct_n(_FwdIt __first, size_t __n) {
	using _Vt = typename iterator_traits<_FwdIt>::value_type;
	for (; __n > 0; (void)++__first, --__n) {
		::new (static_cast<void *>(sprt::addressof(*__first))) _Vt;
	}
	return __first;
}
template <typename _FwdIt>
inline void uninitialized_default_construct(_FwdIt __first, _FwdIt __last) {
	using _Vt = typename iterator_traits<_FwdIt>::value_type;
	for (; __first != __last; ++__first) {
		::new (static_cast<void *>(sprt::addressof(*__first))) _Vt;
	}
}

template <typename _FwdIt>
inline _FwdIt uninitialized_value_construct_n(_FwdIt __first, size_t __n) {
	using _Vt = typename iterator_traits<_FwdIt>::value_type;
	for (; __n > 0; (void)++__first, --__n) {
		::new (static_cast<void *>(sprt::addressof(*__first))) _Vt();
	}
	return __first;
}
template <typename _FwdIt>
inline void uninitialized_value_construct(_FwdIt __first, _FwdIt __last) {
	using _Vt = typename iterator_traits<_FwdIt>::value_type;
	for (; __first != __last; ++__first) {
		::new (static_cast<void *>(sprt::addressof(*__first))) _Vt();
	}
}

template <typename _InIt, typename _FwdIt>
inline _FwdIt uninitialized_copy(_InIt __first, _InIt __last, _FwdIt __d_first) {
	using _Vt = typename iterator_traits<_FwdIt>::value_type;
	for (; __first != __last; ++__first, (void)++__d_first) {
		::new (static_cast<void *>(sprt::addressof(*__d_first))) _Vt(*__first);
	}
	return __d_first;
}
template <typename _InIt, typename _FwdIt>
inline _FwdIt uninitialized_copy_n(_InIt __first, size_t __n, _FwdIt __d_first) {
	using _Vt = typename iterator_traits<_FwdIt>::value_type;
	for (; __n > 0; ++__first, (void)++__d_first, --__n) {
		::new (static_cast<void *>(sprt::addressof(*__d_first))) _Vt(*__first);
	}
	return __d_first;
}

template <typename _InIt, typename _FwdIt>
inline _FwdIt uninitialized_move(_InIt __first, _InIt __last, _FwdIt __d_first) {
	using _Vt = typename iterator_traits<_FwdIt>::value_type;
	for (; __first != __last; ++__first, (void)++__d_first) {
		::new (static_cast<void *>(sprt::addressof(*__d_first))) _Vt(sprt::move_unsafe(*__first));
	}
	return __d_first;
}
template <typename _InIt, typename _FwdIt>
inline pair<_InIt, _FwdIt> uninitialized_move_n(_InIt __first, size_t __n, _FwdIt __d_first) {
	using _Vt = typename iterator_traits<_FwdIt>::value_type;
	for (; __n > 0; ++__first, (void)++__d_first, --__n) {
		::new (static_cast<void *>(sprt::addressof(*__d_first))) _Vt(sprt::move_unsafe(*__first));
	}
	return pair<_InIt, _FwdIt>(__first, __d_first);
}

template <typename _FwdIt, typename _Tp>
inline void uninitialized_fill(_FwdIt __first, _FwdIt __last, const _Tp &__x) {
	using _Vt = typename iterator_traits<_FwdIt>::value_type;
	for (; __first != __last; ++__first) {
		::new (static_cast<void *>(sprt::addressof(*__first))) _Vt(__x);
	}
}
template <typename _FwdIt, typename _Tp>
inline _FwdIt uninitialized_fill_n(_FwdIt __first, size_t __n, const _Tp &__x) {
	using _Vt = typename iterator_traits<_FwdIt>::value_type;
	for (; __n > 0; (void)++__first, --__n) {
		::new (static_cast<void *>(sprt::addressof(*__first))) _Vt(__x);
	}
	return __first;
}

// [ptr.align] adjust ptr up to the next `__alignment` boundary within `__space` bytes.
inline void *align(size_t __alignment, size_t __size, void *&__ptr, size_t &__space) noexcept {
	if (__space < __size) {
		return nullptr;
	}
	const __UINTPTR_TYPE__ __intptr = reinterpret_cast<__UINTPTR_TYPE__>(__ptr);
	const __UINTPTR_TYPE__ __aligned = (__intptr - 1u + __alignment) & -__alignment;
	const __UINTPTR_TYPE__ __diff = __aligned - __intptr;
	if (__diff > (__space - __size)) {
		return nullptr;
	}
	__space -= __diff;
	return __ptr = reinterpret_cast<void *>(__aligned);
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___MEMORY_UNINITIALIZED_H_
