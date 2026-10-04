/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
    10|    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

// Random bytes on Tau come from libtau (sprt_tau.h): not suitable for keys.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_random.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>

namespace sprt {

static __SPRT_ID(ssize_t) getrandom(void *__buffer, __SPRT_ID(size_t) __length, unsigned) {
	if (!__buffer) {
		__sprt_errno = EFAULT;
		return -1;
	}
	__sprt_tau_random(__buffer, __length);
	return (__SPRT_ID(ssize_t))__length;
}

static int getentropy(void *__buffer, __SPRT_ID(size_t) __length) {
	if (__length > 256 || __length == 0 || !__buffer) {
		__sprt_errno = EINVAL;
		return -1;
	}
	__sprt_tau_random(__buffer, __length);
	return 0;
}

} // namespace sprt
