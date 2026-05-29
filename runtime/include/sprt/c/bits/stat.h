#ifndef CORE_RUNTIME_INCLUDE_C_BITS_STAT_H_
#define CORE_RUNTIME_INCLUDE_C_BITS_STAT_H_

#include <sprt/c/bits/__sprt_time_t.h>
#include <sprt/c/bits/__sprt_ssize_t.h>
#include <sprt/c/cross/__sprt_fstypes.h>
#include <sprt/c/cross/__sprt_sysid.h>

#ifdef __SPRT_BUILD
#define __SPRT_STAT_NAME __SPRT_ID(stat)
#else
#define __SPRT_STAT_NAME stat
#endif

struct __SPRT_STAT_NAME {
#include <sprt/c/bits/stat_data.h>
};

#endif // CORE_RUNTIME_INCLUDE_C_BITS_STAT_H_
