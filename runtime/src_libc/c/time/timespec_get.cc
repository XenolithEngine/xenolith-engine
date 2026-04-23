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

#include <sprt/c/__sprt_time.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(timespec_get)(__SPRT_TIMESPEC_NAME *spec, int base) {
#if SPRT_WINDOWS
	struct _timespec64 nativeSpec;
	auto ret = ::_timespec64_get(&nativeSpec, base);
	spec->tv_sec = nativeSpec.tv_sec;
	spec->tv_nsec = nativeSpec.tv_nsec;
	return ret;
#else
	if (base != __SPRT_TIME_UTC) {
		return 0;
	}
	int ret = __sprt_clock_gettime(__SPRT_CLOCK_REALTIME, spec);
	return ret < 0 ? 0 : base;
#endif
}

} // namespace sprt
