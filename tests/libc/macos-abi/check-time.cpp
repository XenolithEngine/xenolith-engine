// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// ---------------------------------------------------------------------------
// cross/macos_sprt/clockid.h + sys/__sprt_time.h <-> Darwin <time.h> parity.
//
// clock_gettime() ids go to libSystem, and struct timespec/timeval cross the
// ABI unrepacked in nanosleep()/select()/utimensat()/setsockopt(SO_RCVTIMEO).
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <time.h>
#include <sys/time.h>

#define SPRT_ABI_HEADER <sprt/c/sys/__sprt_time.h>
// the clockid table is reached through the sysid umbrella
#define SPRT_ABI_HEADER_2 <sprt/c/cross/__sprt_sysid.h>
#include "abi_check.h"

// === clockid_t values ======================================================
SPRT_CONST(CLOCK_REALTIME);
SPRT_CONST(CLOCK_MONOTONIC);
SPRT_CONST(CLOCK_MONOTONIC_RAW);
SPRT_CONST(CLOCK_MONOTONIC_RAW_APPROX);
SPRT_CONST(CLOCK_UPTIME_RAW);
SPRT_CONST(CLOCK_UPTIME_RAW_APPROX);
SPRT_CONST(CLOCK_PROCESS_CPUTIME_ID);
SPRT_CONST(CLOCK_THREAD_CPUTIME_ID);

// Darwin has no CLOCK_MONOTONIC_COARSE / CLOCK_BOOTTIME. sprt keeps the names
// so portable callers compile, mapped onto the nearest Darwin clock; what is
// pinned is that mapping, since the value itself has no native counterpart.
SPRT_CONST_MAP(CLOCK_MONOTONIC_COARSE, CLOCK_MONOTONIC_RAW_APPROX);
SPRT_CONST_MAP(CLOCK_BOOTTIME, CLOCK_UPTIME_RAW);

// === the two time structs ==================================================
SPRT_SIZE(__sprt_timespec, timespec);
SPRT_OFFSET(__sprt_timespec, timespec, tv_sec);
SPRT_OFFSET(__sprt_timespec, timespec, tv_nsec);
SPRT_SIZE(__sprt_timeval, timeval);
SPRT_OFFSET(__sprt_timeval, timeval, tv_sec);
SPRT_OFFSET(__sprt_timeval, timeval, tv_usec);
