#ifndef CORE_RUNTIME_INCLUDE_SPRT_C_BITS_IOVEC_H_
#define CORE_RUNTIME_INCLUDE_SPRT_C_BITS_IOVEC_H_

#include <sprt/c/bits/__sprt_def.h>

#ifdef __SPRT_BUILD
#define __SPRT_IOVEC_NAME __SPRT_ID(iovec)
#else
#define __SPRT_IOVEC_NAME iovec
#endif

struct __SPRT_IOVEC_NAME {
	void *iov_base; /* Starting address */
	size_t iov_len; /* Length in bytes */
};

#endif // CORE_RUNTIME_INCLUDE_SPRT_C_BITS_IOVEC_H_
