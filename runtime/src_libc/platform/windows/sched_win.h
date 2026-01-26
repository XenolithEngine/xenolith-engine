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

#ifndef RUNTIME_SRC_LIBC_PLATFORM_WINDOWS_SCHED_WIN_H_
#define RUNTIME_SRC_LIBC_PLATFORM_WINDOWS_SCHED_WIN_H_

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_stdint.h>
#include <sprt/runtime/int.h>

#include <Windows.h>

namespace sprt {

static constexpr uint32_t __SPRT_SCHED_PRIO_MIN = 0;
static constexpr uint32_t __SPRT_SCHED_PRIO_DEFAULT = 3;
static constexpr uint32_t __SPRT_SCHED_PRIO_MAX = 6;

static constexpr uint32_t __SPRT_SCHED_CLASS_MIN = 0;
static constexpr uint32_t __SPRT_SCHED_CLASS_DEFAULT = 3;
static constexpr uint32_t __SPRT_SCHED_CLASS_MAX = 5;

struct sched_param {
	int sched_priority;
};

static inline int __mapPriority(int32_t posix_prio) {
	switch (posix_prio) {
	case 0: return THREAD_PRIORITY_IDLE; break;
	case 1: return THREAD_PRIORITY_LOWEST; break;
	case 2: return THREAD_PRIORITY_BELOW_NORMAL; break;
	case 3: return THREAD_PRIORITY_NORMAL; break;
	case 4: return THREAD_PRIORITY_ABOVE_NORMAL; break;
	case 5: return THREAD_PRIORITY_HIGHEST; break;
	case 6: return THREAD_PRIORITY_TIME_CRITICAL; break;
	default: break;
	}
	return THREAD_PRIORITY_NORMAL;
}

static inline int __unmapPriority(int32_t posix_prio) {
	switch (posix_prio) {
	case THREAD_PRIORITY_IDLE: return 0; break;
	case THREAD_PRIORITY_LOWEST: return 1; break;
	case THREAD_PRIORITY_BELOW_NORMAL: return 2; break;
	case THREAD_PRIORITY_NORMAL: return 3; break;
	case THREAD_PRIORITY_ABOVE_NORMAL: return 4; break;
	case THREAD_PRIORITY_HIGHEST: return 5; break;
	case THREAD_PRIORITY_TIME_CRITICAL: return 6; break;
	default: break;
	}
	return 3;
}

} // namespace sprt

#endif // RUNTIME_SRC_LIBC_PLATFORM_WINDOWS_SCHED_WIN_H_
