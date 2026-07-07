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

// Replacement for libc++'s <__memory/aligned_alloc.h> (used by libc++abi's
// fallback_malloc). Same signatures as the original; backed by the sprt libc's
// posix_memalign / free (the freestanding, always-available branch).
#ifndef RUNTIME_INCLUDE_LIBC_STL___MEMORY_ALIGNED_ALLOC_H_
#define RUNTIME_INCLUDE_LIBC_STL___MEMORY_ALIGNED_ALLOC_H_

#include <__config>
#include <cstddef> // std::size_t
#include <cstdlib>

_LIBCPP_BEGIN_NAMESPACE_STD

inline _LIBCPP_HIDE_FROM_ABI void *__libcpp_aligned_alloc(std::size_t __alignment, std::size_t __size) {
	void *__result = nullptr;
	(void)::posix_memalign(&__result, __alignment, __size);
	// On failure posix_memalign leaves __result unmodified, so we still return nullptr.
	return __result;
}

inline _LIBCPP_HIDE_FROM_ABI void __libcpp_aligned_free(void *__ptr) { ::free(__ptr); }

_LIBCPP_END_NAMESPACE_STD

#endif // RUNTIME_INCLUDE_LIBC_STL___MEMORY_ALIGNED_ALLOC_H_
