#ifndef CORE_RUNTIME_INCLUDE_SPRT_C_BITS_PTHREAD_H_
#define CORE_RUNTIME_INCLUDE_SPRT_C_BITS_PTHREAD_H_

#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>

#define __SPRT_PTHREAD_COMMON_ALIGNMENT 8

typedef void *__SPRT_ID(pthread_t);

// attr_t (pthread_thread_t.h) is 24 bytes on LP64/ILP32 and 32 under the
// 128-bit pointer mode (its void* stack member doubles); measured sizes,
// asserted in runtime_core_pthread.cpp.
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ > 8
typedef struct SPRT_ALIGNAS(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __data[8];
} __SPRT_ID(pthread_attr_t);
#else
typedef struct SPRT_ALIGNAS(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __data[6];
} __SPRT_ID(pthread_attr_t);
#endif

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
