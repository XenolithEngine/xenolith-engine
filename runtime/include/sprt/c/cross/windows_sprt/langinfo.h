// Freestanding (Windows/WASM) nl_item values. No platform <langinfo.h> to match,
// so these are SPRT's own choice -- a clean sequential layout the libc_impl
// nl_langinfo() dispatches on.
#define __SPRT_CODESET    1
#define __SPRT_D_T_FMT    2
#define __SPRT_D_FMT      3
#define __SPRT_T_FMT      4
#define __SPRT_T_FMT_AMPM 5
#define __SPRT_AM_STR     6
#define __SPRT_PM_STR     7
#define __SPRT_DAY_1      8
#define __SPRT_ABDAY_1    15
#define __SPRT_MON_1      22
#define __SPRT_ABMON_1    34
#define __SPRT_RADIXCHAR  51
#define __SPRT_THOUSEP    52
#define __SPRT_YESEXPR    53
#define __SPRT_NOEXPR     54
#define __SPRT_CRNCYSTR   55
