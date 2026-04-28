#ifndef RUNTIME_MUSL_ADAPTERS_INCLUDE_DEFS_H_
#define RUNTIME_MUSL_ADAPTERS_INCLUDE_DEFS_H_

#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/__sprt_math.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_locale.h>

#ifndef M_E
/*#define M_E __SPRT_M_E
#define M_LOG2E __SPRT_M_LOG2E
#define M_LOG10E __SPRT_M_LOG10E
#define M_LN2 __SPRT_M_LN2
#define M_LN10 __SPRT_M_LN10
#define M_PI __SPRT_M_PI
#define M_PI_2 __SPRT_M_PI_2
#define M_PI_4 __SPRT_M_PI_4
#define M_1_PI __SPRT_M_1_PI
#define M_2_PI__SPRT_M_2_PI
#define M_2_SQRTPI __SPRT_M_2_SQRTPI
#define M_SQRT2 __SPRT_M_SQRT2
#define M_SQRT1_2 __SPRT_M_SQRT1_2*/
#endif


#ifndef __BYTE_ORDER__
#error "__BYTE_ORDER__ is not defined"
#endif

#define __BYTE_ORDER __BYTE_ORDER__
#define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define __BIG_ENDIAN __ORDER_BIG_ENDIAN__


#define weak __attribute__((__weak__))
#define hidden __attribute__((__visibility__("hidden")))
#define weak_alias(old, new) \
	extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))

#endif // RUNTIME_MUSL_ADAPTERS_INCLUDE_DEFS_H_
