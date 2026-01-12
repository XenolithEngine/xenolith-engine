// clang-format off
#define __SPRT__C2          1  // 0 if not 2's complement
#define __SPRT_FP_ILOGB0   (-0x7fffffff - __SPRT__C2)
#define __SPRT_FP_ILOGBNAN 0x7fffffff

#define __SPRT_math_errhandling  (__SPRT_MATH_ERRNO | __SPRT_MATH_ERREXCEPT)

#define __SPRT_FP_INFINITE  1
#define __SPRT_FP_NAN       2
#define __SPRT_FP_NORMAL    (-1)
#define __SPRT_FP_SUBNORMAL (-2)
#define __SPRT_FP_ZERO      0
// clang-format on
