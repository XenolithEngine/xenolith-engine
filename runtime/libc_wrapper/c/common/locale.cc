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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_locale.h>
#include <sprt/c/__sprt_errno.h>

#if __STDC_HOSTED__ == 0
#include "../freestanding/include/locale.h"
#else
#include <locale.h>
#endif

namespace sprt {

__SPRT_C_FUNC char *__SPRT_ID(setlocale)(int cat, const char *locale) {
	return ::setlocale(cat, locale);
}

__SPRT_C_FUNC struct __SPRT_ID(lconv) * __SPRT_ID(localeconv)(void) {
	return (struct __SPRT_ID(lconv) *)::localeconv();
}

__SPRT_C_FUNC __SPRT_ID(locale_t) __SPRT_ID(duplocale)(__SPRT_ID(locale_t) loc) {
	return ::duplocale(loc);
}
__SPRT_C_FUNC void __SPRT_ID(freelocale)(__SPRT_ID(locale_t) loc) { ::freelocale(loc); }
__SPRT_C_FUNC __SPRT_ID(locale_t)
		__SPRT_ID(newlocale)(int v, const char *name, __SPRT_ID(locale_t) loc) {
	return ::newlocale(v, name, loc);
}
__SPRT_C_FUNC __SPRT_ID(locale_t) __SPRT_ID(uselocale)(__SPRT_ID(locale_t) loc) {
	return ::uselocale(loc);
}

} // namespace sprt
