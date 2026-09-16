// Embox shares the Linux/glibc ABI values for most of signal.h, but SIG_DFL /
// SIG_IGN / SIG_ERR are non-null function-pointer sentinels (0x1 / 0x3 / 0x5),
// not glibc's null / 1 / -1. Override after the linux_sprt include.
#include <sprt/c/cross/linux_sprt/signal.h>
#undef __SPRT_SIG_DFL
#undef __SPRT_SIG_IGN
#undef __SPRT_SIG_ERR
#define __SPRT_SIG_DFL ((void (*)(int))0x1)
#define __SPRT_SIG_IGN ((void (*)(int))0x3)
#define __SPRT_SIG_ERR ((void (*)(int))0x5)

// sigprocmask()'s `how` codes. Embox declares SIG_SETMASK and SIG_UNBLOCK as the
// same value (1) - its <signal.h> genuinely does that, it is not a typo here - so
// sprt cannot keep them distinct: the wrapper forwards `how` untranslated, and a
// value Embox does not know would be rejected outright. Mirrored as-is.
#define __SPRT_SIG_BLOCK   0
#define __SPRT_SIG_SETMASK 1
#define __SPRT_SIG_UNBLOCK 1
