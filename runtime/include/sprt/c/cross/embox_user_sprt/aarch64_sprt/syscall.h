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
#define __SPRT_SYSCALL_pread64         67
#define __SPRT_SYSCALL_pwrite64        68

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

// futex: WAIT, WAKE and their BITSET forms with BITSET_MATCH_ANY, private or
// not (there are no mappings shared between tasks). No PI, no requeue, no
// CLOCK_REALTIME -- those answer ENOSYS (ABI doc section 6.2). The first
// syscall that blocks by design.
#define __SPRT_SYSCALL_futex           98

// Threads (K6/L3b). clone is the thread flavour only: CLONE_VM and CLONE_THREAD
// are required and anything outside the pthread set is -EINVAL, because a clone
// without CLONE_VM is fork and fork is never implemented (ABI doc section 6.4).
// set_tid_address registers the word the kernel zeroes and wakes when the
// thread ends -- which is how a joiner learns the thread's stack may be
// unmapped, not how pthread_join waits.
#define __SPRT_SYSCALL_set_tid_address  96
#define __SPRT_SYSCALL_clone           220

// Time. Both were spins here until K6; a sleeping thread that spins holds a
// core, which with real threads is no longer merely wasteful.
#define __SPRT_SYSCALL_nanosleep       101
#define __SPRT_SYSCALL_clock_nanosleep 115
#define __SPRT_SYSCALL_sched_yield     124

// Directories. Embox has no directory descriptor at all: opendir/readdir over a
// DIR*, and its open() ASSERTS on O_DIRECTORY rather than refusing it. So the
// kernel invents the descriptor, out of a reserved high range -- which is why a
// directory fd here is not the ordinary small integer Linux hands back.
#define __SPRT_SYSCALL_getdents64      61

// Paths. All five are built on top of the plain calls, because Embox has almost
// no *at family: the kernel resolves a dirfd by remembering the path behind it.
// Two of them answer differently than a Linux caller may expect, and neither
// difference is detectable from here, so they are written down instead:
// renameat is copy-then-delete and therefore NOT atomic, and readlinkat is
// always EINVAL because no filesystem in the image has symbolic links.
#define __SPRT_SYSCALL_mkdirat          34
#define __SPRT_SYSCALL_unlinkat         35
#define __SPRT_SYSCALL_renameat         38
#define __SPRT_SYSCALL_faccessat        48
#define __SPRT_SYSCALL_readlinkat       78

// The working directory, which under Embox's oldfs is the PWD environment
// variable and nothing else. The environment holds 64-byte rows, so a path
// longer than 59 characters cannot be entered at all: chdir answers
// ENAMETOOLONG well below PATH_MAX.
#define __SPRT_SYSCALL_getcwd           17
#define __SPRT_SYSCALL_chdir            49

// Descriptors. Every fcntl command number differs from Embox's, and the
// collision is the dangerous one -- Linux F_DUPFD is 0, Embox F_GETFD is 0 --
// so the kernel translates rather than forwards. F_GETFL cannot report
// O_CLOEXEC (Embox does not keep it in the flags word); F_GETFD does.
// fsync is a no-op that succeeds: nothing in this image has a write-back
// cache, so there is nothing to force and saying so is not a promise.
#define __SPRT_SYSCALL_dup              23
#define __SPRT_SYSCALL_dup3             24
#define __SPRT_SYSCALL_fcntl            25
#define __SPRT_SYSCALL_ftruncate        46
#define __SPRT_SYSCALL_fsync            82

// Pipes and poll. POLLOUT and POLLPRI are swapped between the two systems and
// the kernel translates both ways; ppoll refuses a non-NULL sigmask, because
// there are no signals here and pretending to block them would be a lie.
#define __SPRT_SYSCALL_pipe2            59
#define __SPRT_SYSCALL_ppoll            73

// getrandom over /dev/urandom, which on this board is an LCG stirred with the
// clock. It is not a CSPRNG, there is no entropy pool, and GRND_RANDOM is
// refused rather than served by the source that is not it.
#define __SPRT_SYSCALL_getrandom       278

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
//   17 getcwd already has a number in the kernel's xl_abi.h but no dispatcher
//   case, so it answers ENOSYS; it stays out of this file until it does not.
//
// M3 - full POSIX profile (K8 and later):
//    130 tkill          131 tgkill         134 rt_sigaction   135 rt_sigprocmask
//    139 rt_sigreturn   216 mremap         233 madvise         99 set_robust_list
//    260 wait4          261 prlimit64      sockets at 198+
