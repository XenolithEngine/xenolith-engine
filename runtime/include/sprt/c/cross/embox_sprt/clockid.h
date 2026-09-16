// Embox numbers its two clocks itself (src/compat/posix/include/time.h):
// CLOCK_MONOTONIC is 1 and CLOCK_REALTIME is 3, so this cannot forward to
// linux_sprt. __sprt_clock_gettime() passes clockid_t through untranslated, and
// outside __SPRT_BUILD these macros ARE the CLOCK_* the application sees, so the
// numbers have to be Embox's own.

// clang-format off
#define __SPRT_CLOCK_MONOTONIC 1
#define __SPRT_CLOCK_REALTIME  3

// Embox aliases CLOCK_MONOTONIC_RAW onto CLOCK_MONOTONIC itself. It has no
// reduced-resolution variants; alias them onto the exact clock they refine, the
// way nuttx_sprt does - callers treat these as "cheaper, possibly coarser", so
// answering with the precise clock is correct, just not cheaper. It also keeps
// SPRuntimePlatform-posix.cc's clock_getres() probe of MONOTONIC_RAW succeeding.
#define __SPRT_CLOCK_MONOTONIC_RAW    __SPRT_CLOCK_MONOTONIC
#define __SPRT_CLOCK_MONOTONIC_COARSE __SPRT_CLOCK_MONOTONIC
#define __SPRT_CLOCK_REALTIME_COARSE  __SPRT_CLOCK_REALTIME

// No counterpart at all on Embox. clockid_t is uint32_t there, so the negative
// ids nuttx_sprt uses for the same purpose are not available; these sit far
// outside the two ids Embox's clock_gettime() accepts, so they fail with EINVAL
// instead of resolving to some other clock.
#define __SPRT_CLOCK_PROCESS_CPUTIME_ID 0x1000
#define __SPRT_CLOCK_THREAD_CPUTIME_ID  0x1001
#define __SPRT_CLOCK_BOOTTIME           0x1002
#define __SPRT_CLOCK_REALTIME_ALARM     0x1003
#define __SPRT_CLOCK_BOOTTIME_ALARM     0x1004
#define __SPRT_CLOCK_SGI_CYCLE          0x1005
#define __SPRT_CLOCK_TAI                0x1006
// clang-format on
