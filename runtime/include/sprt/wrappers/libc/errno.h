#include <sprt/c/__sprt_errno.h>

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#ifndef errno
#define errno __sprt_errno
#endif
#endif
