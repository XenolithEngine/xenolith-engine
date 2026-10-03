#ifndef CORE_RUNTIME_INCLUDE_C_CROSS_TAU_SPRT_SIGNAL_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS_TAU_SPRT_SIGNAL_H_

// Tau userspace uses the Linux generic ABI values for libc types and constants.
#include <sprt/c/cross/linux_sprt/signal.h>

// The freestanding signal() in libc_impl compares against these two sentinels.
#define __SPRT_SIG_GET ((void (*)(int))2)
#define __SPRT_SIG_ACK ((void (*)(int))4)

#endif // CORE_RUNTIME_INCLUDE_C_CROSS_TAU_SPRT_SIGNAL_H_
