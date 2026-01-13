typedef long __SPRT_ID(pthread_t);
typedef int __SPRT_ID(pthread_once_t);
typedef int __SPRT_ID(pthread_key_t);

typedef struct {
#if defined(__LP64__)
	__SPRT_ID(int64_t) __private;
#else
	__SPRT_ID(int32_t) __private[2];
#endif
} __SPRT_ID(pthread_spinlock_t);

typedef long __SPRT_ID(pthread_mutexattr_t);

typedef struct {
#if defined(__LP64__)
	__SPRT_ID(int32_t) __private[12];
#else
	__SPRT_ID(int32_t) __private[1];
#endif
} __SPRT_ID(pthread_cond_t);

typedef long __SPRT_ID(pthread_condattr_t);

typedef long __SPRT_ID(pthread_rwlockattr_t);

typedef int __SPRT_ID(pthread_barrierattr_t);

typedef struct {
#if defined(__LP64__)
	__SPRT_ID(int32_t) __private[10];
#else
	__SPRT_ID(int32_t) __private[1];
#endif
} __SPRT_ID(pthread_mutex_t);

typedef struct {
	__SPRT_ID(uint32_t) flags;
	void *stack_base;
	__SPRT_ID(size_t) stack_size;
	__SPRT_ID(size_t) guard_size;
	__SPRT_ID(int32_t) sched_policy;
	__SPRT_ID(int32_t) sched_priority;
#ifdef __LP64__
	char __reserved[16];
#endif
} __SPRT_ID(pthread_attr_t);

typedef struct {
#if defined(__LP64__)
	__SPRT_ID(int32_t) __private[14];
#else
	__SPRT_ID(int32_t) __private[10];
#endif
} __SPRT_ID(pthread_rwlock_t);

typedef struct {
#if defined(__LP64__)
	__SPRT_ID(int64_t) __private[4];
#else
	__SPRT_ID(int32_t) __private[8];
#endif
} __SPRT_ID(pthread_barrier_t);
