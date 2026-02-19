#if defined(__LP64__)
#define __SPRT_PTHREAD_ATTR_SIZE		56
#define __SPRT_PTHREAD_MUTEXATTR_SIZE	8
#define __SPRT_PTHREAD_MUTEX_SIZE		56
#define __SPRT_PTHREAD_CONDATTR_SIZE	8
#define __SPRT_PTHREAD_COND_SIZE		40
#define __SPRT_PTHREAD_ONCE_SIZE		8
#define __SPRT_PTHREAD_RWLOCK_SIZE		192
#define __SPRT_PTHREAD_RWLOCKATTR_SIZE	16
#else // !__LP64__
#define __SPRT_PTHREAD_ATTR_SIZE		36
#define __SPRT_PTHREAD_MUTEXATTR_SIZE	8
#define __SPRT_PTHREAD_MUTEX_SIZE		40
#define __SPRT_PTHREAD_CONDATTR_SIZE	4
#define __SPRT_PTHREAD_COND_SIZE		24
#define __SPRT_PTHREAD_ONCE_SIZE		4
#define __SPRT_PTHREAD_RWLOCK_SIZE		124
#define __SPRT_PTHREAD_RWLOCKATTR_SIZE	12
#endif // !__LP64__

typedef void *__SPRT_ID(pthread_t);

typedef struct __SPRT_ID(pthread_attr_t) {
	long __sig;
	char __opaque[__SPRT_PTHREAD_ATTR_SIZE];
} __SPRT_ID(pthread_attr_t);

typedef struct __SPRT_ID(pthread_mutexattr_t) {
	long __sig;
	char __opaque[__SPRT_PTHREAD_MUTEXATTR_SIZE];
} __SPRT_ID(pthread_mutexattr_t);

typedef struct __SPRT_ID(pthread_mutex_t) {
	long __sig;
	char __opaque[__SPRT_PTHREAD_MUTEX_SIZE];
} __SPRT_ID(pthread_mutex_t);

typedef struct __SPRT_ID(pthread_condattr_t) {
	long __sig;
	char __opaque[__SPRT_PTHREAD_CONDATTR_SIZE];
} __SPRT_ID(pthread_condattr_t);

typedef struct __SPRT_ID(pthread_cond_t) {
	long __sig;
	char __opaque[__SPRT_PTHREAD_COND_SIZE];
} __SPRT_ID(pthread_cond_t);

typedef struct __SPRT_ID(pthread_once_t) {
	long __sig;
	char __opaque[__SPRT_PTHREAD_ONCE_SIZE];
} __SPRT_ID(pthread_once_t);

typedef struct __SPRT_ID(pthread_rwlockattr_t) {
	long __sig;
	char __opaque[__SPRT_PTHREAD_RWLOCKATTR_SIZE];
} __SPRT_ID(pthread_rwlockattr_t);

typedef struct __SPRT_ID(pthread_rwlock_t) {
	long __sig;
	char __opaque[__SPRT_PTHREAD_RWLOCK_SIZE];
} __SPRT_ID(pthread_rwlock_t);

typedef unsigned long __SPRT_ID(pthread_key_t);

// custom implementation
typedef unsigned __SPRT_ID(pthread_barrierattr_t);

// custom implementation
typedef struct __SPRT_ID(pthread_barrier_t) {
	unsigned __opaque[4];
} __SPRT_ID(pthread_barrier_t);

// custom implementation
typedef unsigned __SPRT_ID(pthread_spinlock_t);
