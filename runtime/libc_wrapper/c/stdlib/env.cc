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

#if SPRT_WINDOWS

#include <sprt/cxx/unordered_map>
#include <sprt/cxx/string>
#include <sprt/wrappers/windows/basic_api.h>

namespace sprt::platform {

int lastErrorToErrno(unsigned long);

}

#endif

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(setenv)(const char *n, const char *v, int r) { return setenv(n, v, r); }

__SPRT_C_FUNC int __SPRT_ID(unsetenv)(const char *n) { return unsetenv(n); }

__SPRT_C_FUNC char *__SPRT_ID(getenv_impl)(const char *name) { return getenv(name); }

} // namespace sprt
