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

#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/cxx/detail/constexpr.h>

#include <stdlib.h>

namespace sprt::libc {

#ifndef SPRT_WINDOWS
static int getenv_s(size_t *pReturnValue, char *buffer, size_t numberOfElements,
		const char *varname) {
	auto val = ::getenv(varname);

	if (!val) {
		if (pReturnValue) {
			*pReturnValue = 0;
		}
		return ESRCH;
	}

	auto len = __constexpr_strlen(val);
	if (pReturnValue) {
		*pReturnValue = len;
	}

	if (numberOfElements < len + 1) {
		__constexpr_memcpy(buffer, val, numberOfElements);
		return ERANGE;
	}

	__constexpr_memcpy(buffer, val, len + 1);
	return 0;
}
#endif

} // namespace sprt::libc

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(setenv)(const char *n, const char *v, int r) { return setenv(n, v, r); }

__SPRT_C_FUNC int __SPRT_ID(unsetenv)(const char *n) { return unsetenv(n); }

__SPRT_C_FUNC char *__SPRT_ID(getenv_impl)(const char *name) { return ::getenv(name); }

__SPRT_C_FUNC int __SPRT_ID(getenv_s)(__SPRT_ID(size_t) * pReturnValue, char *buffer,
		__SPRT_ID(size_t) numberOfElements, const char *varname) {
	return sprt::libc::getenv_s(pReturnValue, buffer, numberOfElements, varname);
}

} // namespace sprt
