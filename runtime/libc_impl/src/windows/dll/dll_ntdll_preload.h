/**
 * Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **/

#ifndef CORE_RUNTIME_LIBC_IMPL_SRC_WINDOWS_DLL_DLL_NTDLL_PRELOAD_H_
#define CORE_RUNTIME_LIBC_IMPL_SRC_WINDOWS_DLL_DLL_NTDLL_PRELOAD_H_

#include "gen/dll_ntdll.h"

namespace sprt {

// The ntdll functions DllTable::init() resolves eagerly are the `preload` of the ntdll DLL in
// runtime/kdl/api; the generated table preloads them and defines them as SPWIN_NTDLL_PRELOAD_LIST.
// The preload is mandatory: if any of these cannot be resolved, init() returns false,
// DllLoader::load() returns LOADER_ERROR_FAIL_TO_PRELOAD, and mainCRTStartup aborts the process
// before any code runs. Consequently these - and only these - functions may be invoked through
// a raw `.fn` pointer without a runtime null check, which SPWIN_NTDLL_PRELOADED_FN() verifies at
// compile time.

constexpr bool __sprt_ntdll_streq(const char *a, const char *b) {
	while (*a && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

// True iff `name` is in SPWIN_NTDLL_PRELOAD_LIST (evaluated at compile time).
constexpr bool isNtdllPreloaded(const char *name) {
#define SPWIN_NTDLL_PRELOAD_NAME_TEST(n) \
	if (__sprt_ntdll_streq(name, #n)) { \
		return true; \
	}
	SPWIN_NTDLL_PRELOAD_LIST(SPWIN_NTDLL_PRELOAD_NAME_TEST)
#undef SPWIN_NTDLL_PRELOAD_NAME_TEST
	return false;
}

template <bool Preloaded>
SPRT_FORCEINLINE FARPROC __sprt_require_preloaded(FARPROC fn) {
	static_assert(Preloaded,
			"this ntdll symbol is dereferenced via a raw .fn pointer but is not in "
			"SPWIN_NTDLL_PRELOAD_LIST. Add it there (so DllTable::init preloads it and the process "
			"aborts at startup if it is missing) or call it through DllTable::call<>().");
	return fn;
}

// Wrap a raw `loaderPtr->ntdll.<member>.fn` access so the preload guarantee that
// makes the missing null-check safe is verified at compile time.
#define SPWIN_NTDLL_PRELOADED_FN(loaderPtr, member) \
	::sprt::__sprt_require_preloaded<::sprt::isNtdllPreloaded(#member)>((loaderPtr)->ntdll.member.fn)

} // namespace sprt

#endif // CORE_RUNTIME_LIBC_IMPL_SRC_WINDOWS_DLL_DLL_NTDLL_PRELOAD_H_
