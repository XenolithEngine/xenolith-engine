// clang-format off
#define __SPRT_FP_ILOGB0      (-2147483647 - 1)
#define __SPRT_FP_ILOGBNAN    (-2147483647 - 1)

#define __SPRT_math_errhandling (__math_errhandling())

__SPRT_BEGIN_DECL
extern int __math_errhandling(void);
__SPRT_END_DECL

#define __SPRT_FP_NAN          1
#define __SPRT_FP_INFINITE     2
#define __SPRT_FP_ZERO         3
#define __SPRT_FP_NORMAL       4
#define __SPRT_FP_SUBNORMAL    5
#define __SPRT_FP_SUPERNORMAL  6 /* legacy PowerPC support; this is otherwise unused */
// clang-format on
