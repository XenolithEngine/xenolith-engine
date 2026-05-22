#include <sprt/c/__sprt_stdarg.h>

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
typedef __SPRT_ID(va_list) va_list;

#ifndef va_start
#define va_start(v, l)   __sprt_va_start(v,l)
#define va_end(v)       __sprt_va_end(v)
#define va_arg(v, l)     __sprt_va_arg(v,l)
#define va_copy(d, s)    __sprt_va_copy(d,s)
#endif
#endif
