#ifndef CORE_RUNTIME_INCLUDE_SPRT_C_BITS_SCHED_PARAM_H_
#define CORE_RUNTIME_INCLUDE_SPRT_C_BITS_SCHED_PARAM_H_

#include <sprt/c/bits/__sprt_def.h>

#ifdef __SPRT_BUILD
#define __SPRT_SCHED_PARAM_NAME __SPRT_ID(sched_param)
#else
#define __SPRT_SCHED_PARAM_NAME sched_param
#endif

struct __SPRT_SCHED_PARAM_NAME {
	int sched_priority;
};

#endif // CORE_RUNTIME_INCLUDE_SPRT_C_BITS_SCHED_PARAM_H_
