// NuttX packs the clock type into the low 3 bits of clockid_t (the rest is a
// pid/fd for the dynamic clocks - see nuttx/clock.h), so it has room for exactly
// six clocks and does not share Linux's numbering past CLOCK_THREAD_CPUTIME_ID.
// __sprt_clock_gettime() forwards clockid_t untranslated, and outside __SPRT_BUILD
// these macros ARE the CLOCK_* the application sees, so the numbers have to be
// NuttX's own.

// clang-format off
#define __SPRT_CLOCK_REALTIME           0
#define __SPRT_CLOCK_MONOTONIC          1
#define __SPRT_CLOCK_PROCESS_CPUTIME_ID 2
#define __SPRT_CLOCK_THREAD_CPUTIME_ID  3
#define __SPRT_CLOCK_BOOTTIME           4 // Linux: 7

// NuttX has no reduced-resolution or raw variants. Alias them to the exact clock
// they refine, the way macos_sprt aliases them onto the *_APPROX clocks: callers
// treat these as "cheaper, possibly coarser", so answering with the precise clock
// is correct, just not cheaper. SPRuntimePlatform-posix.cc probes MONOTONIC_RAW
// with clock_getres() and this makes that probe succeed.
#define __SPRT_CLOCK_MONOTONIC_RAW      __SPRT_CLOCK_MONOTONIC
#define __SPRT_CLOCK_MONOTONIC_COARSE   __SPRT_CLOCK_MONOTONIC
#define __SPRT_CLOCK_REALTIME_COARSE    __SPRT_CLOCK_REALTIME

// No counterpart at all on NuttX. Negative ids cannot collide with a dynamic
// clockid (those are `pid << 3 | type`, and nxclock_gettime() rejects a negative
// pid), so these reliably fail with EINVAL instead of resolving to some other
// clock - which is what 8..11 would do.
#define __SPRT_CLOCK_REALTIME_ALARM    -1
#define __SPRT_CLOCK_BOOTTIME_ALARM    -2
#define __SPRT_CLOCK_SGI_CYCLE         -3
#define __SPRT_CLOCK_TAI               -4
// clang-format on
