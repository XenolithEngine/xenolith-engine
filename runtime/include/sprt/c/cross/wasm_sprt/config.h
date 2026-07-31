// WebAssembly platform configuration (freestanding, browser / wasi host).
//
// wasm has no Linux kernel facilities, so the epoll/eventfd/signalfd/timerfd/
// io_uring readiness mechanisms are all absent. HAVE_FUTEX gates the Linux
// futex(2) *syscall* wrapper specifically, which wasm does not have either — the
// blocking primitive is instead the wasm `memory.atomic.wait32`/`notify`
// instruction set, driven directly by the qlock/rlock layer (see
// runtime/core/wasm/sprt_lock.cc), not through the futex syscall wrapper.

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

// The remainder mirrors the Windows freestanding config: none of these Linux
// kernel APIs exist in wasm, so the libc umbrella routes them to ENOSYS instead
// of calling host symbols that do not exist.

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

// brk/sbrk are implemented over WebAssembly memory.grow (libc_impl/src/wasm/
// unistd.cc); the freestanding wasm allocator (mimalloc's wasi prim) grows the
// OS heap through them.
#ifndef __SPRT_CONFIG_HAVE_UNISTD_BRK
#define __SPRT_CONFIG_HAVE_UNISTD_BRK 1
#endif

#ifndef __SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER
#define __SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER 0
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY
#define __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY 0
#endif

#ifndef __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK
#define __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK 1
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

// The browser sandbox has no pollable descriptors; poll() is an ENOSYS stub.
#ifndef __SPRT_CONFIG_HAVE_POLL
#define __SPRT_CONFIG_HAVE_POLL 0
#endif

#ifndef __SPRT_CONFIG_HAVE_STAT_MKFIFO
#define __SPRT_CONFIG_HAVE_STAT_MKFIFO 0
#endif

#ifndef __SPRT_CONFIG_HAVE_STAT_MKNOD
#define __SPRT_CONFIG_HAVE_STAT_MKNOD 0
#endif
