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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_UTILS_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_UTILS_H_

#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_assert.h>

#include <sprt/cxx/detail/constexpr.h>
#include <sprt/cxx/__algorithm/minmax.h>

namespace sprt::detail {

template <typename T, typename Allocator>
constexpr inline void __allocator_copy(Allocator &allocator, T *dest, const T *source,
		size_t count) noexcept {
	if constexpr (is_trivially_copyable<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else {
		if (dest == source) {
			return;
		} else if (uintptr_t(dest) > uintptr_t(source)) {
			for (size_t i = count; i > 0; i--) {
				allocator.construct(dest + i - 1, *(source + i - 1));
			}
		} else {
			for (size_t i = 0; i < count; i++) {
				allocator.construct(dest + i, *(source + i)); //
			}
		}
	}
}

template <typename T, typename Allocator>
constexpr inline void __allocator_copy_rewrite(Allocator &allocator, T *dest, size_t dcount,
		const T *source, size_t count) noexcept {
	if constexpr (is_trivially_copyable<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else {
		if (dest == source) {
			return;
		} else if (uintptr_t(dest) > uintptr_t(source)) {
			size_t i = count;
			size_t m = min(count, dcount);
			for (; i > m; i--) {
				allocator.construct(dest + i - 1, *(source + i - 1)); //
			}
			for (; i > 0; i--) {
				allocator.destroy(dest + i - 1);
				allocator.construct(dest + i - 1, *(source + i - 1));
			}
		} else {
			size_t i = 0;
			size_t m = min(count, dcount);
			for (; i < m; ++i) {
				allocator.destroy(dest + i);
				allocator.construct(dest + i, *(source + i));
			}
			for (; i < count; ++i) {
				allocator.construct(dest + i, *(source + i)); //
			}
		}
	}
}

template <typename T, typename Allocator>
constexpr inline void __allocator_move(Allocator &allocator, T *dest, T *source,
		size_t count) noexcept {
	if constexpr (is_trivially_copyable<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else if constexpr (is_trivially_move_constructible<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else {
		if (dest == source) {
			return;
		} else if (uintptr_t(dest) > uintptr_t(source)) {
			for (size_t i = count; i > 0; i--) {
				allocator.construct(dest + i - 1, sprt::move_unsafe(*(source + i - 1)));
				allocator.destroy(source + i - 1);
			}
		} else {
			for (size_t i = 0; i < count; i++) {
				allocator.construct(dest + i, sprt::move_unsafe(*(source + i)));
				allocator.destroy(source + i);
			}
		}
	}
}

template <typename T, typename Allocator>
constexpr inline void __allocator_move_rewrite(Allocator &allocator, T *dest, size_t dcount,
		T *source, size_t count) noexcept {
	if constexpr (is_trivially_copyable<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else if constexpr (is_trivially_move_constructible<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else {
		if (dest == source) {
			return;
		} else if (uintptr_t(dest) > uintptr_t(source)) {
			size_t i = count;
			size_t m = min(count, dcount);
			for (; i > m; i--) {
				allocator.construct(dest + i - 1, sprt::move_unsafe(*(source + i - 1)));
				allocator.destroy(source + i - 1);
			}
			for (; i > 0; i--) {
				allocator.destroy(dest + i - 1);
				allocator.construct(dest + i - 1, sprt::move_unsafe(*(source + i - 1)));
				allocator.destroy(source + i - 1);
			}
		} else {
			size_t i = 0;
			size_t m = min(count, dcount);
			for (; i < m; ++i) {
				allocator.destroy(dest + i);
				allocator.construct(dest + i, sprt::move_unsafe(*(source + i)));
				allocator.destroy(source + i);
			}
			for (; i < count; ++i) {
				allocator.construct(dest + i, sprt::move_unsafe(*(source + i)));
				allocator.destroy(source + i);
			}
		}
	}
}

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_UTILS_H_
