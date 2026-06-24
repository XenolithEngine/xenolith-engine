/**
Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#if SPRT_WINDOWS
#include "windows/getrandom.cc"
#else
#include <stdlib.h>
#include <unistd.h>
#if !SPRT_IOS
// iOS ships no <sys/random.h> and does not declare getentropy(); it uses
// SecRandomCopyBytes from Security.framework instead (see below).
#include <sys/random.h>
#endif
#endif

#if SPRT_APPLE
#include <Security/SecRandom.h>
#endif

#if SPRT_ANDROID
#include "android/getrandom.cc"
#endif

namespace sprt {

// Thin pass-through to the platform getentropy(). getentropy() is all-or-nothing
// (it fills the whole buffer or fails, and the platform caps __length at 256), so it
// never returns a partial result and needs no retry loop here.
__SPRT_C_FUNC int __SPRT_ID(getentropy)(void *__buffer, __SPRT_ID(size_t) __length) {
#if SPRT_IOS
	// iOS does not expose getentropy(); SecRandomCopyBytes is all-or-nothing too,
	// matching getentropy()'s 0-on-success / -1-on-failure contract.
	return (SecRandomCopyBytes(kSecRandomDefault, __length, __buffer) == 0) ? 0 : -1;
#else
	return getentropy(__buffer, __length);
#endif
}

// Thin pass-through to the platform CSPRNG; the raw return value is forwarded as-is.
//
// CALLER CONTRACT: this wrapper does NOT loop. On Linux/Android the underlying
// getrandom(2) may return a SHORT count (fewer than __length bytes, e.g. for buffers
// larger than 256 bytes or when interrupted by a signal -> -1/EINTR). A non-negative
// return is the number of bytes actually written and MAY be less than __length, so a
// caller that needs the buffer fully populated must loop until __length bytes have
// been read, retrying on EINTR. (Looping is left to the caller deliberately: this is
// the low-level ABI wrapper; higher-level helpers add the loop.)
//
// On macOS the call is all-or-nothing: SecRandomCopyBytes fills the entire buffer and
// we return __length, or it fails and we return -1. There is no weak/predictable
// fallback on any platform — a hard failure returns -1 (fails closed).
__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(getrandom)(void *__buffer, __SPRT_ID(size_t) __length, unsigned __flags) {
#if SPRT_APPLE
	auto ret = SecRandomCopyBytes(kSecRandomDefault, __length, __buffer);
	if (ret == 0) {
		return __length;
	}
	return -1;
#else
	return getrandom(__buffer, __length, __flags);
#endif
}

} // namespace sprt
