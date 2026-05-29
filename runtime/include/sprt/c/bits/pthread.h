#ifndef CORE_RUNTIME_INCLUDE_SPRT_C_BITS_PTHREAD_H_
#define CORE_RUNTIME_INCLUDE_SPRT_C_BITS_PTHREAD_H_

#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>

#define __SPRT_PTHREAD_COMMON_ALIGNMENT 8

typedef void *__SPRT_ID(pthread_t);

typedef struct SPRT_ALIGNAS(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __data[6];
} __SPRT_ID(pthread_attr_t);

typedef __SPRT_ID(uint32_t) __SPRT_ID(pthread_once_t);

typedef struct SPRT_ALIGNAS(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __data[1];
} __SPRT_ID(pthread_mutexattr_t);

typedef struct SPRT_ALIGNAS(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint64_t) __data[3];
} __SPRT_ID(pthread_mutex_t);

typedef __SPRT_ID(uint32_t) __SPRT_ID(pthread_key_t);

typedef volatile __SPRT_ID(uint32_t) __SPRT_ID(pthread_spinlock_t);

typedef struct SPRT_ALIGNAS(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint64_t) __size[3];
} __SPRT_ID(pthread_cond_t);

typedef struct SPRT_ALIGNAS(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __size[2];
} __SPRT_ID(pthread_condattr_t);

typedef struct SPRT_ALIGNAS(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __size[1];
} __SPRT_ID(pthread_rwlockattr_t);

typedef struct SPRT_ALIGNAS(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __size[1];
} __SPRT_ID(pthread_barrierattr_t);

typedef struct SPRT_ALIGNAS(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __size[5];
} __SPRT_ID(pthread_rwlock_t);

typedef struct SPRT_ALIGNAS(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __size[5];
} __SPRT_ID(pthread_barrier_t);


#endif // CORE_RUNTIME_INCLUDE_SPRT_C_BITS_PTHREAD_H_
