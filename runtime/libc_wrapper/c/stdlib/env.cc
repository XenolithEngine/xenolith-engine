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
// strlen() below: glibc/musl/Bionic pull it in through <stdlib.h>, Embox does not.
#include <string.h>

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

__SPRT_C_FUNC int __SPRT_ID(putenv)(char *s) { return putenv(s); }

__SPRT_C_FUNC char *__SPRT_ID(getenv_impl)(const char *name) { return getenv(name); }

__SPRT_C_FUNC int getenv_s(size_t *ret, char *buf, rsize_t bufSize,
		char const *name) __SPRT_NOEXCEPT {
	if (!ret || (!buf && bufSize > 0) || !name) {
		return EINVAL;
	}
	auto env = getenv(name);
	if (!env) {
		*ret = 0;
		return 0;
	}
	auto len = strlen(env);
	*ret = len + 1;
	if (buf && bufSize < len + 1) {
		return ERANGE;
	}
	if (buf && bufSize >= len + 1) {
		memcpy(buf, env, len + 1);
	}
	return 0;
}

// The allocating member of the same MSVC secure-CRT family as getenv_s: on success *buf
// owns a malloc'd copy the caller frees. A missing variable is not an error - *buf comes
// back null, which is how callers distinguish "unset" from a failure.
__SPRT_C_FUNC int _dupenv_s(char **buf, size_t *bufSize, const char *name) __SPRT_NOEXCEPT {
	if (!buf || !name) {
		return EINVAL;
	}
	*buf = nullptr;
	if (bufSize) {
		*bufSize = 0;
	}
	auto env = getenv(name);
	if (!env) {
		return 0;
	}
	auto len = strlen(env);
	auto mem = static_cast<char *>(malloc(len + 1));
	if (!mem) {
		return ENOMEM;
	}
	memcpy(mem, env, len + 1);
	*buf = mem;
	if (bufSize) {
		*bufSize = len + 1;
	}
	return 0;
}

} // namespace sprt
