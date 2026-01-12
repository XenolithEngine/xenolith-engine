typedef long __SPRT_ID(pthread_t);
typedef int __SPRT_ID(pthread_once_t);
typedef int __SPRT_ID(pthread_key_t);

typedef struct {
#if defined(__LP64__)
	int64_t __private;
#else
	int32_t __private[2];
#endif
} __SPRT_ID(pthread_spinlock_t);

typedef long __SPRT_ID(pthread_mutexattr_t);

typedef struct {
#if defined(__LP64__)
	int32_t __private[12];
#else
	int32_t __private[1];
#endif
} __SPRT_ID(pthread_cond_t);

typedef long __SPRT_ID(pthread_condattr_t);

typedef long __SPRT_ID(pthread_rwlockattr_t);

typedef int __SPRT_ID(pthread_barrierattr_t);

typedef struct {
#if defined(__LP64__)
	int32_t __private[10];
#else
	int32_t __private[1];
#endif
} __SPRT_ID(pthread_mutex_t);

typedef struct {
	uint32_t flags;
	void *stack_base;
	size_t stack_size;
	size_t guard_size;
	int32_t sched_policy;
	int32_t sched_priority;
#ifdef __LP64__
	char __reserved[16];
#endif
} __SPRT_ID(pthread_attr_t);

typedef struct {
#if defined(__LP64__)
	int32_t __private[14];
#else
	int32_t __private[10];
#endif
} __SPRT_ID(pthread_rwlock_t);

typedef struct {
#if defined(__LP64__)
	int64_t __private[4];
#else
	int32_t __private[8];
#endif
} __SPRT_ID(pthread_barrier_t);
