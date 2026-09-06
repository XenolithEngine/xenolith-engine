
// Embox EL0 clock backend.
//
// One syscall, clock_gettime(113), and the ABI's clock ids go across unchanged
// (CLOCK_MONOTONIC 1, CLOCK_REALTIME 0 -- the Linux numbering, which the kernel
// translates into Embox's own 1/3 on its side).
//
// clock_getres is answered locally: the resolution is a property of the timer the
// kernel drives, and there is no syscall that reports it. armv8_phy_timer runs at
// the rate the template's core_freq sets and Embox's jiffies counter ticks at
// 1 kHz, so the honest answer for both clocks is one millisecond -- NOT one
// nanosecond, which is what a struct timespec could express and what a caller
// would wrongly conclude from clock_gettime's units alone.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/c/cross/__sprt_syscall.h>

#include "../include/__el0_syscall.h"

namespace sprt {

// Embox's clock_gettime resolves to jiffies, which the aarch64/qemu and Pi 4
// templates both run at 1 kHz (embox.kernel.time.jiffies over armv8_phy_timer).
static constexpr long EL0_CLOCK_RES_NS = 1'000'000L;

__SPRT_C_FUNC int clock_gettime(__SPRT_ID(clockid_t) clk_id,
		struct __SPRT_TIMESPEC_NAME *tp) __SPRT_NOEXCEPT {
	if (!tp) {
		__sprt_errno = EFAULT;
		return -1;
	}
	// The kernel writes the timespec directly into EL0 memory; its layout is
	// two 64-bit words, which is what struct timespec is here (ABI doc 4.5).
	return (int)__el0_ret(__el0_clock_gettime((int)clk_id, tp));
}

__SPRT_C_FUNC int clock_getres(__SPRT_ID(clockid_t) clk_id,
		struct __SPRT_TIMESPEC_NAME *tp) __SPRT_NOEXCEPT {
	if (!tp) {
		__sprt_errno = EFAULT;
		return -1;
	}
	switch (clk_id) {
	case __SPRT_CLOCK_REALTIME:
	case __SPRT_CLOCK_MONOTONIC:
		tp->tv_sec = 0;
		tp->tv_nsec = EL0_CLOCK_RES_NS;
		return 0;
	default:
		// Every other id would be rejected by clock_gettime too; say so here
		// rather than report a resolution for a clock that cannot be read.
		__sprt_errno = EINVAL;
		return -1;
	}
}

// clock_settime(112) is not in the table and is not planned: setting the wall
// clock from an unprivileged EL0 task is not something this system offers.
static int _clock_settime(unsigned clk_id, const struct __SPRT_TIMESPEC_NAME *ts) {
	(void)clk_id;
	(void)ts;
	__sprt_errno = ENOSYS;
	return -1;
}

} // namespace sprt
