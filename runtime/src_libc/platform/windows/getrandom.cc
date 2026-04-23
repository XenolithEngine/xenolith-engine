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

#include <sprt/c/sys/__sprt_random.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/cxx/detail/ctypes.h>

namespace sprt {

static ssize_t getrandom(void *__buffer, size_t __length, unsigned flags) {
	auto ret = BCryptGenRandom(nullptr, (unsigned char *)__buffer, __length,
			BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (ret == 0) {
		return __length;
	}
	return -1;
}

static int getentropy(void *__buffer, __SPRT_ID(size_t) __length) {
	if (__length > 256 || __length == 0 || !__buffer) {
		__sprt_errno = EINVAL;
		return -1;
	}

	return __sprt_getrandom(__buffer, __length, __SPRT_GRND_RANDOM);
}

} // namespace sprt
