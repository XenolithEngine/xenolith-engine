/* We need to provide structures that correspond to the largest possible size for their implementation in the  platform's libc */

#define __SPRT_PTHREAD_COMMON_ALIGNMENT 8

#define __SPRT_SIZEOF_PTHREAD_MUTEXATTR_T 8
#define __SPRT_SIZEOF_PTHREAD_COND_T 56
#define __SPRT_SIZEOF_PTHREAD_CONDATTR_T 8
#define __SPRT_SIZEOF_PTHREAD_RWLOCKATTR_T 8
#define __SPRT_SIZEOF_PTHREAD_BARRIERATTR_T 8

#ifdef __x86_64__
#if defined(__LP64__)
#define __SPRT_SIZEOF_PTHREAD_MUTEX_T 40
#define __SPRT_SIZEOF_PTHREAD_ATTR_T 56
#define __SPRT_SIZEOF_PTHREAD_RWLOCK_T 56
#define __SPRT_SIZEOF_PTHREAD_BARRIER_T 32
#else
#define __SPRT_SIZEOF_PTHREAD_MUTEX_T 32
#define __SPRT_SIZEOF_PTHREAD_ATTR_T 32
#define __SPRT_SIZEOF_PTHREAD_RWLOCK_T 44
#define __SPRT_SIZEOF_PTHREAD_BARRIER_T 20
#endif
#else
#define __SPRT_SIZEOF_PTHREAD_MUTEX_T 24
#define __SPRT_SIZEOF_PTHREAD_ATTR_T 36
#define __SPRT_SIZEOF_PTHREAD_RWLOCK_T 32
#define __SPRT_SIZEOF_PTHREAD_BARRIER_T 20
#endif

typedef unsigned long __SPRT_ID(pthread_t);
typedef int __SPRT_ID(pthread_once_t);
typedef unsigned int __SPRT_ID(pthread_key_t);
typedef volatile int __SPRT_ID(pthread_spinlock_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_MUTEXATTR_T];
} __SPRT_ID(pthread_mutexattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_COND_T];
} __SPRT_ID(pthread_cond_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_CONDATTR_T];
} __SPRT_ID(pthread_condattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_RWLOCKATTR_T];
} __SPRT_ID(pthread_rwlockattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_BARRIERATTR_T];
} __SPRT_ID(pthread_barrierattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_MUTEX_T];
} __SPRT_ID(pthread_mutex_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_ATTR_T];
} __SPRT_ID(pthread_attr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_RWLOCK_T];
} __SPRT_ID(pthread_rwlock_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_BARRIER_T];
} __SPRT_ID(pthread_barrier_t);
