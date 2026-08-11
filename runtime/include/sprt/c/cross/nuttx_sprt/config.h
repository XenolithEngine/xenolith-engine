// NuttX platform configuration (hosted POSIX on the NuttX RTOS kernel).
//
// NuttX gives us a real libc with pthread/sem/mqueue/poll/select, but it does
// NOT provide the Linux-specific readiness APIs the runtime knows about:
//   * no epoll   — NuttX has poll() but not epoll_create/ctl/wait
//   * no eventfd, signalfd, timerfd, io_uring
//   * no futex(2) syscall wrapper — sprt_lock uses sem_t instead
//                  (see runtime/core/nuttx/sprt_lock.cc)
// HAVE_* gates here mirror make/os/nuttx.mk and the libc_wrapper routing.

// NuttX <time.h> gates tzset() behind CONFIG_LIBC_LOCALTIME, <nuttx/lib/setjmp.h>
// gates sigsetjmp_buf_s behind CONFIG_ARCH_SETJMP_H, and the exported .config
// carries these as "y" strings rather than defined macros. Force-define both so
// the declarations the sprt runtime relies on are visible.
#ifndef CONFIG_LIBC_LOCALTIME
#define CONFIG_LIBC_LOCALTIME 1
#endif
#ifndef CONFIG_ARCH_SETJMP_H
#define CONFIG_ARCH_SETJMP_H 1
#endif

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

#ifndef __SPRT_CONFIG_HAVE_FUTEX
#define __SPRT_CONFIG_HAVE_FUTEX 0
#endif

// NuttX POSIX layer: real poll, pthread, signal, sem, mqueue.
#ifndef __SPRT_CONFIG_HAVE_POLL
#define __SPRT_CONFIG_HAVE_POLL 1
#endif

#ifndef __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK
#define __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK 1
#endif

// The Linux-specific fcntl / unistd extensions are absent; the libc umbrella
// routes them to ENOSYS rather than calling missing host symbols.
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

#ifndef __SPRT_CONFIG_HAVE_UNISTD_COPY_FILE_RANGE
#define __SPRT_CONFIG_HAVE_UNISTD_COPY_FILE_RANGE 0
#endif

#ifndef __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM
#define __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM 0
#endif

#ifndef __SPRT_CONFIG_HAVE_TIME_CLOCK_SETTIME
#define __SPRT_CONFIG_HAVE_TIME_CLOCK_SETTIME 0
#endif

#ifndef __SPRT_CONFIG_HAVE_TIME_TIMER
#define __SPRT_CONFIG_HAVE_TIME_TIMER 0
#endif

#ifndef __SPRT_CONFIG_HAVE_TIME_ADJTIME
#define __SPRT_CONFIG_HAVE_TIME_ADJTIME 0
#endif

// NuttX has no multi-user security model in flat builds.
#ifndef __SPRT_CONFIG_HAVE_UNISTD_CHOWN
#define __SPRT_CONFIG_HAVE_UNISTD_CHOWN 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_DUP3
#define __SPRT_CONFIG_HAVE_UNISTD_DUP3 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_NICE
#define __SPRT_CONFIG_HAVE_UNISTD_NICE 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
#define __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID 0
#endif

// No fork/exec in a flat build — posix_spawn / task_spawn exist instead.
#ifndef __SPRT_CONFIG_HAVE_UNISTD_FORK
#define __SPRT_CONFIG_HAVE_UNISTD_FORK 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_FEXEC
#define __SPRT_CONFIG_HAVE_UNISTD_FEXEC 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_EXEC
#define __SPRT_CONFIG_HAVE_UNISTD_EXEC 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_SETLOGIN
#define __SPRT_CONFIG_HAVE_UNISTD_SETLOGIN 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME
#define __SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_GETPPID
#define __SPRT_CONFIG_HAVE_UNISTD_GETPPID 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_SETHOSTNAME
#define __SPRT_CONFIG_HAVE_UNISTD_SETHOSTNAME 0
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_BRK
#define __SPRT_CONFIG_HAVE_UNISTD_BRK 0
#endif

#ifndef __SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER
#define __SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER 0
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY
#define __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY 0
#endif

#ifndef __SPRT_CONFIG_HAVE_MMAN_MLOCKALL
#define __SPRT_CONFIG_HAVE_MMAN_MLOCKALL 0
#endif

#ifndef __SPRT_CONFIG_HAVE_MMAN_MREMAP
#define __SPRT_CONFIG_HAVE_MMAN_MREMAP 0
#endif

#ifndef __SPRT_CONFIG_HAVE_MMAN_MEMFD
#define __SPRT_CONFIG_HAVE_MMAN_MEMFD 0
#endif

#ifndef __SPRT_CONFIG_HAVE_STAT_MKFIFO
#define __SPRT_CONFIG_HAVE_STAT_MKFIFO 0
#endif

#ifndef __SPRT_CONFIG_HAVE_STAT_MKNOD
#define __SPRT_CONFIG_HAVE_STAT_MKNOD 0
#endif
