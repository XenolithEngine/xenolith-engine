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

#include <stdio.h>
#include <stdlib.h>
#include <sprt/cxx/new>

#if SPRT_WINDOWS
#include "windows/libcxx.cc"
#include "windows/Demangle/MicrosoftDemangle.cc"
#include "windows/Demangle/MicrosoftDemangleNodes.cc"
#endif

namespace std {
enum class align_val_t : sprt::size_t {
};
} // namespace std

void *operator new(size_t __blockLen) { return ::malloc(__blockLen); }

void *operator new[](size_t __blockLen) { return ::malloc(__blockLen); }

void *operator new(size_t __blockLen, std::align_val_t align) {
	return ::aligned_alloc(__blockLen, sprt::to_underlying(align));
}

void *operator new[](size_t __blockLen, std::align_val_t align) {
	return ::aligned_alloc(__blockLen, sprt::to_underlying(align));
}

void operator delete(void *ptr) noexcept { return ::free(ptr); }

void operator delete(void *ptr, size_t sz) noexcept { return ::free_sized(ptr, sz); }

void operator delete[](void *ptr) noexcept { return ::free(ptr); }

void operator delete[](void *ptr, size_t sz) noexcept { return ::free_sized(ptr, sz); }

void operator delete(void *ptr, size_t sz, std::align_val_t al) noexcept {
	return free_aligned_sized(ptr, sprt::to_underlying(al), sz);
}

void operator delete[](void *ptr, size_t sz, std::align_val_t al) noexcept {
	return free_aligned_sized(ptr, sprt::to_underlying(al), sz);
}

__SPRT_C_FUNC FILE *stderr;

// MS C++ ABI function
__SPRT_C_FUNC void _purecall(void) {
// Platform-specific debug break
#ifdef _WIN32
	__debugbreak(); // Break into debugger
#else
	__builtin_debugtrap();
#endif
	fprintf(stderr, "Fatal: %s; aborting;\n", "Pure virtual function called");
	abort();
}

// General C++ ABI function
__SPRT_C_FUNC __attribute__((weak)) void __cxa_pure_virtual(void) { _purecall(); }
