// Embox user-mode (EL0) platform configuration.
//
// The application is a static ET_EXEC on its own freestanding libc
// (runtime/libc_impl); the only thing it shares with the kernel is the syscall
// boundary. So every gate below answers exactly one question: does the EL0
// syscall table implement this today?
//
// That table is small on purpose and grows by milestone (xenolith-os
// docs/EMBOX-USERSPACE.md section 5). The gates marked "M2"/"M3"/"K<n>" are the
// ones expected to flip; each names the phase that flips it, and flipping one
// without the matching entry in aarch64_sprt/syscall.h will not compile - the
// syscall number simply is not declared.
//
// A gate at 0 does not mean "broken": the umbrella in include_libc/ then routes
// the call to ENOSYS (and, with __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS, warns
// at the call site) instead of emitting a call that would trap.

// --- Readiness mechanisms: Linux-kernel-specific, never coming to Embox. ---

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

// futex(98) is real since K6, and with it the locks stopped spinning. The
// wrappers in libc_wrapper/sys/SPRuntimeCSysFutex.cpp reach it through
// __el0_futex rather than syscall(2), which this target does not have; the PI
// operations and futex2 answer ENOSYS, because the kernel does not offer them
// (docs/EMBOX-SYSCALL-ABI.md section 6.2).
#ifndef __SPRT_CONFIG_HAVE_FUTEX
#define __SPRT_CONFIG_HAVE_FUTEX 1
#endif

// ppoll(73) landed with M2. poll() is built on it here rather than the other way
// round: the kernel has only the timespec form, and a millisecond timeout is the
// lossy one to derive.
#ifndef __SPRT_CONFIG_HAVE_POLL
#define __SPRT_CONFIG_HAVE_POLL 1
#endif

// --- Process model: absent by design, not by phase. ---
//
// Decision D5 fixes the application as a single static ET_EXEC with no loader
// and no second process. fork/exec are not "not yet"; there is nothing for them
// to do, and the kernel-side task is created by the ELF loader (K7), not by a
// syscall from EL0.

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

// --- Credentials, hostname, scheduling: no syscall, and no kernel state to ask. ---

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

// Embox's scheduler has no affinity mask to set (M3 at the earliest).
#ifndef __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY
#define __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY 0
#endif

// --- Descriptors and file plumbing. ---

// dup3(24) landed with M2, and dup(23) with it. Note what a duplicate is here:
// the KERNEL descriptor is duplicated too, because a libc slot holds a kernel fd
// number rather than a reference-counted object -- two slots sharing one number
// would be two closes of the same descriptor.
#ifndef __SPRT_CONFIG_HAVE_UNISTD_DUP3
#define __SPRT_CONFIG_HAVE_UNISTD_DUP3 1
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

// mknodat(33)/mkfifoat: nothing on the Embox side to create them with.
#ifndef __SPRT_CONFIG_HAVE_STAT_MKFIFO
#define __SPRT_CONFIG_HAVE_STAT_MKFIFO 0
#endif

#ifndef __SPRT_CONFIG_HAVE_STAT_MKNOD
#define __SPRT_CONFIG_HAVE_STAT_MKNOD 0
#endif

// --- Memory. ---

// brk(214) IS implemented - it is how the allocator bootstraps before mmap is
// usable (ABI doc section 2). One of the few gates here that is on.
#ifndef __SPRT_CONFIG_HAVE_UNISTD_BRK
#define __SPRT_CONFIG_HAVE_UNISTD_BRK 1
#endif

// mremap(216) is M3. K5's region list can grow a mapping in place, but there is
// no syscall in front of it yet.
#ifndef __SPRT_CONFIG_HAVE_MMAN_MREMAP
#define __SPRT_CONFIG_HAVE_MMAN_MREMAP 0
#endif

// Every EL0 page is already resident and never paged out, so mlockall would be a
// no-op that lies. memfd has no backing object on Embox.
#ifndef __SPRT_CONFIG_HAVE_MMAN_MLOCKALL
#define __SPRT_CONFIG_HAVE_MMAN_MLOCKALL 0
#endif

#ifndef __SPRT_CONFIG_HAVE_MMAN_MEMFD
#define __SPRT_CONFIG_HAVE_MMAN_MEMFD 0
#endif

// --- Time. ---
//
// clock_gettime(113) is implemented and is not gated - it is the one the engine's
// frame timing needs. What follows is everything around it that is not.

#ifndef __SPRT_CONFIG_HAVE_TIME_CLOCK_SETTIME
#define __SPRT_CONFIG_HAVE_TIME_CLOCK_SETTIME 0
#endif

#ifndef __SPRT_CONFIG_HAVE_TIME_TIMER
#define __SPRT_CONFIG_HAVE_TIME_TIMER 0
#endif

#ifndef __SPRT_CONFIG_HAVE_TIME_ADJTIME
#define __SPRT_CONFIG_HAVE_TIME_ADJTIME 0
#endif

// --- Pure-libc gates: these ask about libc_impl, not about the kernel. ---

// sigprocmask and friends operate on our own per-thread mask
// (libc_impl/src/builtin_signal.cpp) and reach no syscall, so they work now -
// well before signal DELIVERY exists (K8).
#ifndef __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK
#define __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK 1
#endif

// Despite the name this gate guards open_wmemstream only (include_libc/wchar.h).
// libc_impl builds stdio/open_memstream.cc but not musl's open_wmemstream.c, so
// the wide variant is genuinely absent - the same answer wasm gives, for the same
// reason. Revisit here if builtin_wstdio_musl.cpp ever picks it up.
#ifndef __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM
#define __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM 0
#endif
