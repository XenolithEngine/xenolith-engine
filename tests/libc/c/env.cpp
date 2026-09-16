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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "test_util.h"

namespace sprt::test {

// getenv / setenv / unsetenv / putenv over the process environment. Uses a single
// uniquely named variable so the process's real environment (which differs between
// the host and the freestanding targets) never leaks into the output.
void performEnvTest() {
	const char *K = "SPRT_LIBC_ENV_TEST";
	unsetenv(K); // start from a known-absent state

	const char *v = getenv(K);
	printf("get(absent)=%s\n", v ? v : "(null)");

	// setenv a fresh variable
	printf("setenv(one)=%d\n", setenv(K, "one", 0));
	v = getenv(K);
	printf("get=%s\n", v ? v : "(null)");

	// setenv without overwrite must keep the existing value
	printf("setenv(two,noover)=%d\n", setenv(K, "two", 0));
	v = getenv(K);
	printf("get(noover)=%s\n", v ? v : "(null)");

	// setenv with overwrite replaces it
	printf("setenv(three,over)=%d\n", setenv(K, "three", 1));
	v = getenv(K);
	printf("get(over)=%s\n", v ? v : "(null)");

	// unsetenv removes it; a second get misses
	printf("unsetenv=%d\n", unsetenv(K));
	v = getenv(K);
	printf("get(unset)=%s\n", v ? v : "(null)");

	// putenv installs a "name=value" string (static storage: putenv keeps the pointer)
	static char pe[] = "SPRT_LIBC_ENV_TEST=four";
	printf("putenv=%d\n", putenv(pe));
	v = getenv(K);
	printf("get(putenv)=%s\n", v ? v : "(null)");

	// putenv with a bare name (no '=') removes the variable
	static char pn[] = "SPRT_LIBC_ENV_TEST";
	printf("putenv(remove)=%d\n", putenv(pn));
	v = getenv(K);
	printf("get(removed)=%s\n", v ? v : "(null)");

	// getenv of an empty name misses
	printf("get(empty)=%s\n", getenv("") ? "set" : "null");

	// setenv rejects an invalid (contains '=') name with EINVAL
	errno = 0;
	int bad = setenv("a=b", "x", 1);
	printf("setenv(bad)=%d errno=%s\n", bad, errnoName(errno));

	unsetenv(K); // leave the environment clean
}

} // namespace sprt::test
