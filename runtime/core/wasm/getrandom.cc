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

// WebAssembly CSPRNG backend.
//
// Entropy comes from a single typed host import (T1 SYNC): `random_get(buf,len)`
// backed by crypto.getRandomValues, which is available in workers. The host
// fills the whole buffer or fails; there is no weak fallback (fails closed).

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_random.h>
#include <sprt/c/__sprt_errno.h>

extern "C" {
// T1 host import: fill [buf, buf+len) with cryptographically strong bytes.
// Returns 0 on success, -errno on failure. The JS side chunks calls to the
// 65536-byte crypto.getRandomValues limit.
__attribute__((import_module("sprt"), import_name("random_get"))) int __sprt_host_random_get(
		void *buf, __SPRT_ID(size_t) len);
}

namespace sprt {

static __SPRT_ID(ssize_t) getrandom(void *__buffer, __SPRT_ID(size_t) __length, unsigned flags) {
	int ret = __sprt_host_random_get(__buffer, __length);
	if (ret == 0) {
		return static_cast<__SPRT_ID(ssize_t)>(__length);
	}
	__sprt_errno = -ret;
	return -1;
}

static int getentropy(void *__buffer, __SPRT_ID(size_t) __length) {
	if (__length > 256 || __length == 0 || !__buffer) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return static_cast<int>(__sprt_getrandom(__buffer, __length, __SPRT_GRND_RANDOM)) < 0 ? -1 : 0;
}

} // namespace sprt
