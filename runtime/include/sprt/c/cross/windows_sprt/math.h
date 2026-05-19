// clang-format off
#define __SPRT_FP_ILOGBNAN (-1-0x7fffffff)
#define __SPRT_FP_ILOGB0 __SPRT_FP_ILOGBNAN

#ifdef __FAST_MATH__
#define __SPRT_math_errhandling	0
#elif defined __NO_MATH_ERRNO__
#define __SPRT_math_errhandling	(__SPRT_MATH_ERREXCEPT)
#else
#define __SPRT_math_errhandling	(__SPRT_MATH_ERRNO | __SPRT_MATH_ERREXCEPT)
#endif

#define __SPRT_FP_NAN       0
#define __SPRT_FP_INFINITE  1
#define __SPRT_FP_ZERO      2
#define __SPRT_FP_SUBNORMAL 3
#define __SPRT_FP_NORMAL    4
// clang-format on
