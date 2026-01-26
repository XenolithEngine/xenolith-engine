#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>

#define __SPRT_PTHREAD_COMMON_ALIGNMENT 8

// clang-format off
#define __SPRT_PTHREAD_KEYS_MAX (0xFFFFFFFFu - 1u)
#define __SPRT_PTHREAD_DESTRUCTOR_ITERATIONS 4
// clang-format on

typedef void *__SPRT_ID(pthread_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __data[2];
} __SPRT_ID(pthread_attr_t);

typedef __SPRT_ID(uint32_t) __SPRT_ID(pthread_once_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __data[1];
} __SPRT_ID(pthread_mutexattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint64_t) __data[2];
} __SPRT_ID(pthread_mutex_t);

typedef __SPRT_ID(uint32_t) __SPRT_ID(pthread_key_t);

typedef volatile __SPRT_ID(uint32_t) __SPRT_ID(pthread_spinlock_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint64_t) __size[3];
} __SPRT_ID(pthread_cond_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __size[1];
} __SPRT_ID(pthread_condattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __size[1];
} __SPRT_ID(pthread_rwlockattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __size[1];
} __SPRT_ID(pthread_barrierattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __size[4];
} __SPRT_ID(pthread_rwlock_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	__SPRT_ID(uint32_t) __size[4];
} __SPRT_ID(pthread_barrier_t);
