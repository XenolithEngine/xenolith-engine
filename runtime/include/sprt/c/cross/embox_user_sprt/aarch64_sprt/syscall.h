// The EL0 syscall table of the Embox user-mode target.
//
// Numbers are Linux/aarch64 generic ABI values (decision D1) and are a strict
// SUBSET of linux_sprt/aarch64_sprt/syscall.h - never a renumbering. The kernel
// side is xenolith-os board/embox-qemu/drivers/xlsyscall/; the two are pinned
// against each other and against the Linux table by
// xenolith-os scripts/check-abi.py, which fails if this file names a syscall the
// dispatcher does not implement, or gives one a number Linux does not use.
//
// WHAT IS ABSENT IS ABSENT. A syscall the kernel does not implement gets no
// #define here at all - not a -1, not a sentinel. Two reasons:
//
//   * A reference to an undeclared __SPRT_SYSCALL_x is a compile error naming
//     the exact symbol, which is the whole point: the alternative is a build
//     that succeeds and then answers ENOSYS at run time on a device.
//   * Feature detection in the runtime is written as `#ifdef
//     __SPRT_SYSCALL_inotify_init1` (see src/dispatch/platform/fd/). Defining an
//     absent call as -1 would make every one of those tests pass.
//
// Adding a syscall is therefore a two-line change - the dispatcher case, then
// its line moved out of the roadmap below - and the checker refuses either half
// alone.
//
// clang-format off

// --- M1: implemented and exercised in QEMU. ---

// I/O.
#define __SPRT_SYSCALL_ioctl           29
#define __SPRT_SYSCALL_openat          56
#define __SPRT_SYSCALL_close           57
#define __SPRT_SYSCALL_lseek           62
#define __SPRT_SYSCALL_read            63
#define __SPRT_SYSCALL_write           64
#define __SPRT_SYSCALL_readv           65
#define __SPRT_SYSCALL_writev          66

// stat. Both spellings exist because they carry DIFFERENT argument shapes, not
// because one is legacy: fstat takes (fd, buf), newfstatat takes
// (dirfd, path, buf, flags). Both fill the same 128-byte struct kstat, whose
// layout is fixed in ABI doc section 4.1 and asserted on the kernel side.
#define __SPRT_SYSCALL_newfstatat      79
#define __SPRT_SYSCALL_fstat           80

// Process lifetime. exit/exit_group are answered in the trap handler itself
// (patches/fpsimd/sync_handler.c), before the dispatcher runs - they must unwind
// the EL0 thread rather than return a value to it.
#define __SPRT_SYSCALL_exit            93
#define __SPRT_SYSCALL_exit_group      94

#define __SPRT_SYSCALL_clock_gettime  113
#define __SPRT_SYSCALL_uname          160
#define __SPRT_SYSCALL_getpid         172
#define __SPRT_SYSCALL_gettid         178

// Memory. brk bootstraps the allocator; mmap/munmap/mprotect are what mimalloc
// actually runs on.
#define __SPRT_SYSCALL_brk            214
#define __SPRT_SYSCALL_munmap         215
#define __SPRT_SYSCALL_mmap           222
#define __SPRT_SYSCALL_mprotect       226

// clang-format on

// --- Roadmap. Not declared, hence not callable. ---
//
// These are the numbers the calls WILL have, recorded here so that implementing
// one is a move rather than a lookup, and so that nothing gets accidentally
// numbered twice (ABI doc section 9 forbids ever reusing a number).
//
// M2 - "kiosk with a picture and threads" (K6, K7):
//     17 getcwd          23 dup             24 dup3            25 fcntl
//     34 mkdirat         35 unlinkat        38 renameat        46 ftruncate
//     48 faccessat       49 chdir           59 pipe2           61 getdents64
//     73 ppoll           78 readlinkat      82 fsync           96 set_tid_address
//     98 futex          101 nanosleep      115 clock_nanosleep 124 sched_yield
//    220 clone          278 getrandom
//
//   17 getcwd already has a number in the kernel's xl_abi.h but no dispatcher
//   case, so it answers ENOSYS; it stays out of this file until it does not.
//
// M3 - full POSIX profile (K8 and later):
//    130 tkill          131 tgkill         134 rt_sigaction   135 rt_sigprocmask
//    139 rt_sigreturn   216 mremap         233 madvise         99 set_robust_list
//    260 wait4          261 prlimit64      sockets at 198+
