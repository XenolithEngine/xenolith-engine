#ifndef CORE_RUNTIME_INCLUDE_SPRT_C_BITS_SCHED_PARAM_H_
#define CORE_RUNTIME_INCLUDE_SPRT_C_BITS_SCHED_PARAM_H_

#include <sprt/c/bits/__sprt_def.h>

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1
#define __SPRT_SCHED_PARAM_NAME __SPRT_ID(sched_param)
#else
#define __SPRT_SCHED_PARAM_NAME sched_param
#endif

struct __SPRT_SCHED_PARAM_NAME {
	int sched_priority;
};

#endif // CORE_RUNTIME_INCLUDE_SPRT_C_BITS_SCHED_PARAM_H_
