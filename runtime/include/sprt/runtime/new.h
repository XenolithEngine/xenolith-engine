/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_NEW_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_NEW_H_

#include <sprt/runtime/new_utils.h>
#include <sprt/c/__sprt_stdlib.h>

[[nodiscard]]
inline void *operator new(sprt::size_t, void *__p) noexcept {
	return __p;
}

[[nodiscard]]
inline void *operator new[](sprt::size_t, void *__p) noexcept {
	return __p;
}

[[nodiscard]]
inline constexpr void *operator new(sprt::size_t size, sprt::nothrow_t) noexcept {
	if (size == 0) {
		size = 1;
	}
	return __sprt_malloc(size);
}

[[nodiscard]]
inline constexpr void *operator new[](sprt::size_t size, sprt::nothrow_t) noexcept {
	if (size == 0) {
		size = 1;
	}
	return __sprt_malloc(size);
}

inline void operator delete(void *, void *) noexcept { }
inline void operator delete[](void *, void *) noexcept { }

inline void operator delete(void *ptr, const sprt::nothrow_t &) noexcept { __sprt_free(ptr); }
inline void operator delete[](void *ptr, const sprt::nothrow_t &) noexcept { __sprt_free(ptr); }

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_NEW_H_
