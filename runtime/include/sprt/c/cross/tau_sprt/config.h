// Tau userspace platform configuration.
//
// A module is a static ELF on libc_impl; it reaches the kernel and other
// modules only through libtau (capability invocations and shared rings).
// A gate at 0 routes the call to ENOSYS through include_libc/; a gate flips
// to 1 when libtau gains the service behind it.

// Linux-kernel readiness mechanisms.

#ifndef __SPRT_CONFIG_HAVE_EPOLL
#define __SPRT_CONFIG_HAVE_EPOLL 0
#endif

#ifndef __SPRT_CONFIG_HAVE_EVENTFD
#define __SPRT_CONFIG_HAVE_EVENTFD 0
#endif

#ifndef __SPRT_CONFIG_HAVE_SIGNALFD
#define __SPRT_CONFIG_HAVE_SIGNALFD 0
#endif

#ifndef __SPRT_CONFIG_HAVE_TIMERFD
#define __SPRT_CONFIG_HAVE_TIMERFD 0
#endif

#ifndef __SPRT_CONFIG_HAVE_URING
#define __SPRT_CONFIG_HAVE_URING 0
#endif

// The module-private futex over notifications and poll over the module's streams
// (sprt_tau.h).
#ifndef __SPRT_CONFIG_HAVE_FUTEX
#define __SPRT_CONFIG_HAVE_FUTEX 1
#endif

#ifndef __SPRT_CONFIG_HAVE_POLL
#define __SPRT_CONFIG_HAVE_POLL 1
#endif

// Process model: modules are created by the root supervisor, never by fork/exec.

#ifndef __SPRT_CONFIG_HAVE_UNISTD_FORK
#define __SPRT_CONFIG_HAVE_UNISTD_FORK 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_EXEC
#define __SPRT_CONFIG_HAVE_UNISTD_EXEC 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_FEXEC
#define __SPRT_CONFIG_HAVE_UNISTD_FEXEC 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_GETPPID
#define __SPRT_CONFIG_HAVE_UNISTD_GETPPID 0
#endif

// Credentials, hostname, scheduling knobs: no such kernel state.

#ifndef __SPRT_CONFIG_HAVE_UNISTD_CHOWN
#define __SPRT_CONFIG_HAVE_UNISTD_CHOWN 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
#define __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_SETLOGIN
#define __SPRT_CONFIG_HAVE_UNISTD_SETLOGIN 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_SETHOSTNAME
#define __SPRT_CONFIG_HAVE_UNISTD_SETHOSTNAME 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME
#define __SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_NICE
#define __SPRT_CONFIG_HAVE_UNISTD_NICE 0
#endif

#ifndef __SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER
#define __SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER 0
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY
#define __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY 0
#endif

// Descriptors and file plumbing.

#ifndef __SPRT_CONFIG_HAVE_UNISTD_DUP3
#define __SPRT_CONFIG_HAVE_UNISTD_DUP3 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_COPY_FILE_RANGE
#define __SPRT_CONFIG_HAVE_UNISTD_COPY_FILE_RANGE 0
#endif

#ifndef __SPRT_CONFIG_HAVE_FCNTL_SPLICE
#define __SPRT_CONFIG_HAVE_FCNTL_SPLICE 0
#endif

#ifndef __SPRT_CONFIG_HAVE_FCNTL_TEE
#define __SPRT_CONFIG_HAVE_FCNTL_TEE 0
#endif

#ifndef __SPRT_CONFIG_HAVE_FCNTL_FALLOCATE
#define __SPRT_CONFIG_HAVE_FCNTL_FALLOCATE 0
#endif

#ifndef __SPRT_CONFIG_HAVE_FCNTL_FADVICE
#define __SPRT_CONFIG_HAVE_FCNTL_FADVICE 0
#endif

#ifndef __SPRT_CONFIG_HAVE_FCNTL_READAHEAD
#define __SPRT_CONFIG_HAVE_FCNTL_READAHEAD 0
#endif

#ifndef __SPRT_CONFIG_HAVE_FCNTL_SYNC_FILE_RANGE
#define __SPRT_CONFIG_HAVE_FCNTL_SYNC_FILE_RANGE 0
#endif

#ifndef __SPRT_CONFIG_HAVE_STAT_MKFIFO
#define __SPRT_CONFIG_HAVE_STAT_MKFIFO 0
#endif

#ifndef __SPRT_CONFIG_HAVE_STAT_MKNOD
#define __SPRT_CONFIG_HAVE_STAT_MKNOD 0
#endif

// Memory: the heap grows inside the region the root grants in StartInfo.

#ifndef __SPRT_CONFIG_HAVE_UNISTD_BRK
#define __SPRT_CONFIG_HAVE_UNISTD_BRK 0
#endif

#ifndef __SPRT_CONFIG_HAVE_MMAN_MREMAP
#define __SPRT_CONFIG_HAVE_MMAN_MREMAP 0
#endif

// Module memory is resident and never paged out.
#ifndef __SPRT_CONFIG_HAVE_MMAN_MLOCKALL
#define __SPRT_CONFIG_HAVE_MMAN_MLOCKALL 0
#endif

#ifndef __SPRT_CONFIG_HAVE_MMAN_MEMFD
#define __SPRT_CONFIG_HAVE_MMAN_MEMFD 0
#endif

// Time: clock_gettime reads CNTVCT_EL0 directly; everything else needs timer@1.

#ifndef __SPRT_CONFIG_HAVE_TIME_CLOCK_SETTIME
#define __SPRT_CONFIG_HAVE_TIME_CLOCK_SETTIME 0
#endif

#ifndef __SPRT_CONFIG_HAVE_TIME_TIMER
#define __SPRT_CONFIG_HAVE_TIME_TIMER 0
#endif

#ifndef __SPRT_CONFIG_HAVE_TIME_ADJTIME
#define __SPRT_CONFIG_HAVE_TIME_ADJTIME 0
#endif

// Pure libc_impl gates.

// The per-thread signal mask is libc_impl state and reaches no kernel call.
#ifndef __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK
#define __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK 1
#endif

// Guards open_wmemstream only; libc_impl does not build it.
#ifndef __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM
#define __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM 0
#endif

#ifndef __SPRT_CONFIG_HAVE_FLOCK
#define __SPRT_CONFIG_HAVE_FLOCK 0
#endif
