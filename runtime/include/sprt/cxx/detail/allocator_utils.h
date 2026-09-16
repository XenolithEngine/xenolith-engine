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

// Note: __constexpr_memmove takes an ELEMENT count, not a byte count (it
// multiplies by sizeof(T) internally / iterates `count` elements). Passing the
// element `count` here is correct and matches the non-trivial branches below.
template <typename T, typename Allocator>
constexpr inline void __allocator_copy(Allocator &allocator, T *dest, const T *source,
		size_t count) noexcept {
	if (is_constant_evaluated()) {
		// In constant evaluation the destination is (possibly) raw storage, where
		// assignment is ill-formed; construct each element instead. Callers use this
		// only for disjoint ranges, so iteration order is irrelevant.
		if (dest == source) {
			return;
		}
		for (size_t i = 0; i < count; ++i) { allocator.construct(dest + i, *(source + i)); }
		return;
	}
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
	if (is_constant_evaluated()) {
		// The first dcount elements are live (re-construct over them), the rest are
		// raw storage. Disjoint source, so forward iteration is safe.
		if (dest == source) { // self-assign: contents already in place
			return;
		}
		size_t m = min(count, dcount);
		size_t i = 0;
		for (; i < m; ++i) {
			allocator.destroy(dest + i);
			allocator.construct(dest + i, *(source + i));
		}
		for (; i < count; ++i) { allocator.construct(dest + i, *(source + i)); }
		return;
	}
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
	if (is_constant_evaluated()) {
		// Relocate element-wise via construction (assignment on raw storage is
		// ill-formed in constant evaluation). Forward iteration is correct for the
		// disjoint relocations on the constexpr-exercised path.
		if (dest == source) {
			return;
		}
		for (size_t i = 0; i < count; ++i) {
			allocator.construct(dest + i, sprt::move_unsafe(*(source + i)));
			allocator.destroy(source + i);
		}
		return;
	}
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
	if (is_constant_evaluated()) {
		if (dest == source) {
			return;
		}
		size_t m = min(count, dcount);
		size_t i = 0;
		for (; i < m; ++i) {
			allocator.destroy(dest + i);
			allocator.construct(dest + i, sprt::move_unsafe(*(source + i)));
			allocator.destroy(source + i);
		}
		for (; i < count; ++i) {
			allocator.construct(dest + i, sprt::move_unsafe(*(source + i)));
			allocator.destroy(source + i);
		}
		return;
	}
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

// Move `count` elements between two ranges that lie in the SAME allocation (the
// in-place shifts used by insert/emplace/erase). Because both pointers address the
// same array object, `dest > source` is a valid comparison in constant evaluation
// (unlike the disjoint relocations handled by __allocator_move), so the correct
// iteration direction can be chosen there. At runtime this is a plain overlap-safe
// memmove for trivially relocatable elements.
template <typename T, typename Allocator>
constexpr inline void __allocator_move_within(Allocator &allocator, T *dest, T *source,
		size_t count) noexcept {
	if (is_constant_evaluated()) {
		if (dest == source || count == 0) {
			return;
		}
		if (dest > source) {
			// shift right: go backward so each source element is read (and each dest
			// slot has already been vacated by an earlier step) before being touched.
			for (size_t i = count; i > 0; --i) {
				allocator.construct(dest + i - 1, sprt::move_unsafe(*(source + i - 1)));
				allocator.destroy(source + i - 1);
			}
		} else {
			// shift left: forward is the safe direction.
			for (size_t i = 0; i < count; ++i) {
				allocator.construct(dest + i, sprt::move_unsafe(*(source + i)));
				allocator.destroy(source + i);
			}
		}
		return;
	}
	__allocator_move(allocator, dest, source, count);
}

// Detect whether `p` points inside the array [base, base + count). If so, report its
// index in `off` and return true. Used to keep the source pointer of a self-aliasing
// append/insert/replace valid across a reallocation: after the buffer moves, the data
// lives at base_new + off. In constant evaluation only pointer EQUALITY is usable
// (relational comparison of unrelated pointers is not a constant expression), so scan
// the range element-by-element there; at runtime a direct range check suffices.
template <typename T>
constexpr inline bool __ptr_index_within(const T *base, size_t count, const T *p,
		size_t &off) noexcept {
	if (base == nullptr) {
		return false;
	}
	if (is_constant_evaluated()) {
		for (size_t i = 0; i < count; ++i) {
			if (base + i == p) {
				off = i;
				return true;
			}
		}
		return false;
	} else {
		if (p >= base && p < base + count) {
			off = static_cast<size_t>(p - base);
			return true;
		}
		return false;
	}
}

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_UTILS_H_
