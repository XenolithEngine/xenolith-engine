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

// No-op <pwd.h> account-database lookups for the freestanding targets (Windows,
// wasm). Neither platform has an /etc/passwd user database. The one consumer is
// musl's glob() (built in the musl-adapters regex SCU): its GLOB_TILDE path calls
// getpwnam_r (for "~user") and getpwuid_r (for "~" when $HOME is unset) to find a
// home directory. The reentrant stubs report "no such user" — return 0 with
// *result = NULL — which makes glob() yield GLOB_NOMATCH for the tilde, exactly
// as a POSIX host does for a user that does not exist. The non-reentrant
// getpwnam/getpwuid are stubbed too (return NULL). getuid() and getenv(), the
// other tilde inputs, are already provided by the unistd / stdlib builtins.

#include <sprt/c/__sprt_unistd.h>

// Opaque: the stubs never construct or dereference a passwd record.
struct passwd;

extern "C" struct passwd *getpwnam(const char *) __SPRT_NOEXCEPT { return nullptr; }

extern "C" struct passwd *getpwuid(__SPRT_ID(uid_t)) __SPRT_NOEXCEPT { return nullptr; }

extern "C" int getpwnam_r(const char *, struct passwd *, char *,
		__SPRT_ID(size_t), struct passwd **__result) __SPRT_NOEXCEPT {
	if (__result) {
		*__result = nullptr;
	}
	return 0;
}

extern "C" int getpwuid_r(__SPRT_ID(uid_t), struct passwd *, char *,
		__SPRT_ID(size_t), struct passwd **__result) __SPRT_NOEXCEPT {
	if (__result) {
		*__result = nullptr;
	}
	return 0;
}
