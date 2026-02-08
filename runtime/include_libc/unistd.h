/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef CORE_RUNTIME_INCLUDE_LIBC_UNISTD_H_
#define CORE_RUNTIME_INCLUDE_LIBC_UNISTD_H_

#ifdef __SPRT_BUILD

#include_next <unistd.h>

#else

#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_stdarg.h>

#ifndef SEEK_SET
#define SEEK_SET __SPRT_SEEK_SET
#define SEEK_CUR __SPRT_SEEK_CUR
#define SEEK_END __SPRT_SEEK_END
#endif

#ifndef R_OK
#define R_OK __SPRT_R_OK
#define W_OK __SPRT_W_OK
#define X_OK __SPRT_X_OK
#define F_OK __SPRT_F_OK
#endif

#ifndef AT_FDCWD
#define AT_FDCWD __SPRT_AT_FDCWD
#define AT_SYMLINK_NOFOLLOW __SPRT_AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_FOLLOW __SPRT_AT_SYMLINK_FOLLOW
#define AT_NO_AUTOMOUNT __SPRT_AT_NO_AUTOMOUNT
#define AT_EMPTY_PATH __SPRT_AT_EMPTY_PATH
#define AT_STATX_SYNC_TYPE __SPRT_AT_STATX_SYNC_TYPE
#define AT_STATX_SYNC_AS_STAT __SPRT_AT_STATX_SYNC_AS_STAT
#define AT_STATX_FORCE_SYNC __SPRT_AT_STATX_FORCE_SYNC
#define AT_STATX_DONT_SYNC __SPRT_AT_STATX_DONT_SYNC
#define AT_RECURSIVE __SPRT_AT_RECURSIVE
#define AT_RENAME_NOREPLACE __SPRT_AT_RENAME_NOREPLACE
#define AT_RENAME_EXCHANGE __SPRT_AT_RENAME_EXCHANGE
#define AT_RENAME_WHITEOUT __SPRT_AT_RENAME_WHITEOUT
#define AT_EACCESS __SPRT_AT_EACCESS
#define AT_REMOVEDIR __SPRT_AT_REMOVEDIR
#define AT_HANDLE_FID __SPRT_AT_HANDLE_FID
#define AT_HANDLE_MNT_ID_UNIQUE __SPRT_AT_HANDLE_MNT_ID_UNIQUE
#endif

#ifdef __SPRT_SC_ARG_MAX
#define SC_ARG_MAX __SPRT_SC_ARG_MAX
#endif
#ifdef __SPRT_SC_CHILD_MAX
#define SC_CHILD_MAX __SPRT_SC_CHILD_MAX
#endif
#ifdef __SPRT_SC_CLK_TCK
#define SC_CLK_TCK __SPRT_SC_CLK_TCK
#endif
#ifdef __SPRT_SC_NGROUPS_MAX
#define SC_NGROUPS_MAX __SPRT_SC_NGROUPS_MAX
#endif
#ifdef __SPRT_SC_OPEN_MAX
#define SC_OPEN_MAX __SPRT_SC_OPEN_MAX
#endif
#ifdef __SPRT_SC_STREAM_MAX
#define SC_STREAM_MAX __SPRT_SC_STREAM_MAX
#endif
#ifdef __SPRT_SC_TZNAME_MAX
#define SC_TZNAME_MAX __SPRT_SC_TZNAME_MAX
#endif
#ifdef __SPRT_SC_JOB_CONTROL
#define SC_JOB_CONTROL __SPRT_SC_JOB_CONTROL
#endif
#ifdef __SPRT_SC_SAVED_IDS
#define SC_SAVED_IDS __SPRT_SC_SAVED_IDS
#endif
#ifdef __SPRT_SC_REALTIME_SIGNALS
#define SC_REALTIME_SIGNALS __SPRT_SC_REALTIME_SIGNALS
#endif
#ifdef __SPRT_SC_PRIORITY_SCHEDULING
#define SC_PRIORITY_SCHEDULING __SPRT_SC_PRIORITY_SCHEDULING
#endif
#ifdef __SPRT_SC_TIMERS
#define SC_TIMERS __SPRT_SC_TIMERS
#endif
#ifdef __SPRT_SC_ASYNCHRONOUS_IO
#define SC_ASYNCHRONOUS_IO __SPRT_SC_ASYNCHRONOUS_IO
#endif
#ifdef __SPRT_SC_PRIORITIZED_IO
#define SC_PRIORITIZED_IO __SPRT_SC_PRIORITIZED_IO
#endif
#ifdef __SPRT_SC_SYNCHRONIZED_IO
#define SC_SYNCHRONIZED_IO __SPRT_SC_SYNCHRONIZED_IO
#endif
#ifdef __SPRT_SC_FSYNC
#define SC_FSYNC __SPRT_SC_FSYNC
#endif
#ifdef __SPRT_SC_MAPPED_FILES
#define SC_MAPPED_FILES __SPRT_SC_MAPPED_FILES
#endif
#ifdef __SPRT_SC_MEMLOCK
#define SC_MEMLOCK __SPRT_SC_MEMLOCK
#endif
#ifdef __SPRT_SC_MEMLOCK_RANGE
#define SC_MEMLOCK_RANGE __SPRT_SC_MEMLOCK_RANGE
#endif
#ifdef __SPRT_SC_MEMORY_PROTECTION
#define SC_MEMORY_PROTECTION __SPRT_SC_MEMORY_PROTECTION
#endif
#ifdef __SPRT_SC_MESSAGE_PASSING
#define SC_MESSAGE_PASSING __SPRT_SC_MESSAGE_PASSING
#endif
#ifdef __SPRT_SC_SEMAPHORES
#define SC_SEMAPHORES __SPRT_SC_SEMAPHORES
#endif
#ifdef __SPRT_SC_SHARED_MEMORY_OBJECTS
#define SC_SHARED_MEMORY_OBJECTS __SPRT_SC_SHARED_MEMORY_OBJECTS
#endif
#ifdef __SPRT_SC_AIO_LISTIO_MAX
#define SC_AIO_LISTIO_MAX __SPRT_SC_AIO_LISTIO_MAX
#endif
#ifdef __SPRT_SC_AIO_MAX
#define SC_AIO_MAX __SPRT_SC_AIO_MAX
#endif
#ifdef __SPRT_SC_AIO_PRIO_DELTA_MAX
#define SC_AIO_PRIO_DELTA_MAX __SPRT_SC_AIO_PRIO_DELTA_MAX
#endif
#ifdef __SPRT_SC_DELAYTIMER_MAX
#define SC_DELAYTIMER_MAX __SPRT_SC_DELAYTIMER_MAX
#endif
#ifdef __SPRT_SC_MQ_OPEN_MAX
#define SC_MQ_OPEN_MAX __SPRT_SC_MQ_OPEN_MAX
#endif
#ifdef __SPRT_SC_MQ_PRIO_MAX
#define SC_MQ_PRIO_MAX __SPRT_SC_MQ_PRIO_MAX
#endif
#ifdef __SPRT_SC_VERSION
#define SC_VERSION __SPRT_SC_VERSION
#endif
#ifdef __SPRT_SC_PAGE_SIZE
#define SC_PAGE_SIZE __SPRT_SC_PAGE_SIZE
#endif
#ifdef __SPRT_SC_PAGESIZE
#define SC_PAGESIZE __SPRT_SC_PAGESIZE
#endif
#ifdef __SPRT_SC_RTSIG_MAX
#define SC_RTSIG_MAX __SPRT_SC_RTSIG_MAX
#endif
#ifdef __SPRT_SC_SEM_NSEMS_MAX
#define SC_SEM_NSEMS_MAX __SPRT_SC_SEM_NSEMS_MAX
#endif
#ifdef __SPRT_SC_SEM_VALUE_MAX
#define SC_SEM_VALUE_MAX __SPRT_SC_SEM_VALUE_MAX
#endif
#ifdef __SPRT_SC_SIGQUEUE_MAX
#define SC_SIGQUEUE_MAX __SPRT_SC_SIGQUEUE_MAX
#endif
#ifdef __SPRT_SC_TIMER_MAX
#define SC_TIMER_MAX __SPRT_SC_TIMER_MAX
#endif
#ifdef __SPRT_SC_BC_BASE_MAX
#define SC_BC_BASE_MAX __SPRT_SC_BC_BASE_MAX
#endif
#ifdef __SPRT_SC_BC_DIM_MAX
#define SC_BC_DIM_MAX __SPRT_SC_BC_DIM_MAX
#endif
#ifdef __SPRT_SC_BC_SCALE_MAX
#define SC_BC_SCALE_MAX __SPRT_SC_BC_SCALE_MAX
#endif
#ifdef __SPRT_SC_BC_STRING_MAX
#define SC_BC_STRING_MAX __SPRT_SC_BC_STRING_MAX
#endif
#ifdef __SPRT_SC_COLL_WEIGHTS_MAX
#define SC_COLL_WEIGHTS_MAX __SPRT_SC_COLL_WEIGHTS_MAX
#endif
#ifdef __SPRT_SC_EXPR_NEST_MAX
#define SC_EXPR_NEST_MAX __SPRT_SC_EXPR_NEST_MAX
#endif
#ifdef __SPRT_SC_LINE_MAX
#define SC_LINE_MAX __SPRT_SC_LINE_MAX
#endif
#ifdef __SPRT_SC_RE_DUP_MAX
#define SC_RE_DUP_MAX __SPRT_SC_RE_DUP_MAX
#endif
#ifdef __SPRT_SC_2_VERSION
#define SC_2_VERSION __SPRT_SC_2_VERSION
#endif
#ifdef __SPRT_SC_2_C_BIND
#define SC_2_C_BIND __SPRT_SC_2_C_BIND
#endif
#ifdef __SPRT_SC_2_C_DEV
#define SC_2_C_DEV __SPRT_SC_2_C_DEV
#endif
#ifdef __SPRT_SC_2_FORT_DEV
#define SC_2_FORT_DEV __SPRT_SC_2_FORT_DEV
#endif
#ifdef __SPRT_SC_2_FORT_RUN
#define SC_2_FORT_RUN __SPRT_SC_2_FORT_RUN
#endif
#ifdef __SPRT_SC_2_SW_DEV
#define SC_2_SW_DEV __SPRT_SC_2_SW_DEV
#endif
#ifdef __SPRT_SC_2_LOCALEDEF
#define SC_2_LOCALEDEF __SPRT_SC_2_LOCALEDEF
#endif
#ifdef __SPRT_SC_UIO_MAXIOV
#define SC_UIO_MAXIOV __SPRT_SC_UIO_MAXIOV
#endif
#ifdef __SPRT_SC_IOV_MAX
#define SC_IOV_MAX __SPRT_SC_IOV_MAX
#endif
#ifdef __SPRT_SC_THREADS
#define SC_THREADS __SPRT_SC_THREADS
#endif
#ifdef __SPRT_SC_THREAD_SAFE_FUNCTIONS
#define SC_THREAD_SAFE_FUNCTIONS __SPRT_SC_THREAD_SAFE_FUNCTIONS
#endif
#ifdef __SPRT_SC_GETGR_R_SIZE_MAX
#define SC_GETGR_R_SIZE_MAX __SPRT_SC_GETGR_R_SIZE_MAX
#endif
#ifdef __SPRT_SC_GETPW_R_SIZE_MAX
#define SC_GETPW_R_SIZE_MAX __SPRT_SC_GETPW_R_SIZE_MAX
#endif
#ifdef __SPRT_SC_LOGIN_NAME_MAX
#define SC_LOGIN_NAME_MAX __SPRT_SC_LOGIN_NAME_MAX
#endif
#ifdef __SPRT_SC_TTY_NAME_MAX
#define SC_TTY_NAME_MAX __SPRT_SC_TTY_NAME_MAX
#endif
#ifdef __SPRT_SC_THREAD_DESTRUCTOR_ITERATIONS
#define SC_THREAD_DESTRUCTOR_ITERATIONS __SPRT_SC_THREAD_DESTRUCTOR_ITERATIONS
#endif
#ifdef __SPRT_SC_THREAD_KEYS_MAX
#define SC_THREAD_KEYS_MAX __SPRT_SC_THREAD_KEYS_MAX
#endif
#ifdef __SPRT_SC_THREAD_STACK_MIN
#define SC_THREAD_STACK_MIN __SPRT_SC_THREAD_STACK_MIN
#endif
#ifdef __SPRT_SC_THREAD_THREADS_MAX
#define SC_THREAD_THREADS_MAX __SPRT_SC_THREAD_THREADS_MAX
#endif
#ifdef __SPRT_SC_THREAD_ATTR_STACKADDR
#define SC_THREAD_ATTR_STACKADDR __SPRT_SC_THREAD_ATTR_STACKADDR
#endif
#ifdef __SPRT_SC_THREAD_ATTR_STACKSIZE
#define SC_THREAD_ATTR_STACKSIZE __SPRT_SC_THREAD_ATTR_STACKSIZE
#endif
#ifdef __SPRT_SC_THREAD_PRIORITY_SCHEDULING
#define SC_THREAD_PRIORITY_SCHEDULING __SPRT_SC_THREAD_PRIORITY_SCHEDULING
#endif
#ifdef __SPRT_SC_THREAD_PRIO_INHERIT
#define SC_THREAD_PRIO_INHERIT __SPRT_SC_THREAD_PRIO_INHERIT
#endif
#ifdef __SPRT_SC_THREAD_PRIO_PROTECT
#define SC_THREAD_PRIO_PROTECT __SPRT_SC_THREAD_PRIO_PROTECT
#endif
#ifdef __SPRT_SC_THREAD_PROCESS_SHARED
#define SC_THREAD_PROCESS_SHARED __SPRT_SC_THREAD_PROCESS_SHARED
#endif
#ifdef __SPRT_SC_NPROCESSORS_CONF
#define SC_NPROCESSORS_CONF __SPRT_SC_NPROCESSORS_CONF
#endif
#ifdef __SPRT_SC_NPROCESSORS_ONLN
#define SC_NPROCESSORS_ONLN __SPRT_SC_NPROCESSORS_ONLN
#endif
#ifdef __SPRT_SC_PHYS_PAGES
#define SC_PHYS_PAGES __SPRT_SC_PHYS_PAGES
#endif
#ifdef __SPRT_SC_AVPHYS_PAGES
#define SC_AVPHYS_PAGES __SPRT_SC_AVPHYS_PAGES
#endif
#ifdef __SPRT_SC_ATEXIT_MAX
#define SC_ATEXIT_MAX __SPRT_SC_ATEXIT_MAX
#endif
#ifdef __SPRT_SC_PASS_MAX
#define SC_PASS_MAX __SPRT_SC_PASS_MAX
#endif
#ifdef __SPRT_SC_XOPEN_VERSION
#define SC_XOPEN_VERSION __SPRT_SC_XOPEN_VERSION
#endif
#ifdef __SPRT_SC_XOPEN_XCU_VERSION
#define SC_XOPEN_XCU_VERSION __SPRT_SC_XOPEN_XCU_VERSION
#endif
#ifdef __SPRT_SC_XOPEN_UNIX
#define SC_XOPEN_UNIX __SPRT_SC_XOPEN_UNIX
#endif
#ifdef __SPRT_SC_XOPEN_CRYPT
#define SC_XOPEN_CRYPT __SPRT_SC_XOPEN_CRYPT
#endif
#ifdef __SPRT_SC_XOPEN_ENH_I18N
#define SC_XOPEN_ENH_I18N __SPRT_SC_XOPEN_ENH_I18N
#endif
#ifdef __SPRT_SC_XOPEN_SHM
#define SC_XOPEN_SHM __SPRT_SC_XOPEN_SHM
#endif
#ifdef __SPRT_SC_2_CHAR_TERM
#define SC_2_CHAR_TERM __SPRT_SC_2_CHAR_TERM
#endif
#ifdef __SPRT_SC_2_UPE
#define SC_2_UPE __SPRT_SC_2_UPE
#endif
#ifdef __SPRT_SC_XOPEN_XPG2
#define SC_XOPEN_XPG2 __SPRT_SC_XOPEN_XPG2
#endif
#ifdef __SPRT_SC_XOPEN_XPG3
#define SC_XOPEN_XPG3 __SPRT_SC_XOPEN_XPG3
#endif
#ifdef __SPRT_SC_XOPEN_XPG4
#define SC_XOPEN_XPG4 __SPRT_SC_XOPEN_XPG4
#endif
#ifdef __SPRT_SC_NZERO
#define SC_NZERO __SPRT_SC_NZERO
#endif
#ifdef __SPRT_SC_XBS5_ILP32_OFF32
#define SC_XBS5_ILP32_OFF32 __SPRT_SC_XBS5_ILP32_OFF32
#endif
#ifdef __SPRT_SC_XBS5_ILP32_OFFBIG
#define SC_XBS5_ILP32_OFFBIG __SPRT_SC_XBS5_ILP32_OFFBIG
#endif
#ifdef __SPRT_SC_XBS5_LP64_OFF64
#define SC_XBS5_LP64_OFF64 __SPRT_SC_XBS5_LP64_OFF64
#endif
#ifdef __SPRT_SC_XBS5_LPBIG_OFFBIG
#define SC_XBS5_LPBIG_OFFBIG __SPRT_SC_XBS5_LPBIG_OFFBIG
#endif
#ifdef __SPRT_SC_XOPEN_LEGACY
#define SC_XOPEN_LEGACY __SPRT_SC_XOPEN_LEGACY
#endif
#ifdef __SPRT_SC_XOPEN_REALTIME
#define SC_XOPEN_REALTIME __SPRT_SC_XOPEN_REALTIME
#endif
#ifdef __SPRT_SC_XOPEN_REALTIME_THREADS
#define SC_XOPEN_REALTIME_THREADS __SPRT_SC_XOPEN_REALTIME_THREADS
#endif
#ifdef __SPRT_SC_ADVISORY_INFO
#define SC_ADVISORY_INFO __SPRT_SC_ADVISORY_INFO
#endif
#ifdef __SPRT_SC_BARRIERS
#define SC_BARRIERS __SPRT_SC_BARRIERS
#endif
#ifdef __SPRT_SC_CLOCK_SELECTION
#define SC_CLOCK_SELECTION __SPRT_SC_CLOCK_SELECTION
#endif
#ifdef __SPRT_SC_CPUTIME
#define SC_CPUTIME __SPRT_SC_CPUTIME
#endif
#ifdef __SPRT_SC_THREAD_CPUTIME
#define SC_THREAD_CPUTIME __SPRT_SC_THREAD_CPUTIME
#endif
#ifdef __SPRT_SC_MONOTONIC_CLOCK
#define SC_MONOTONIC_CLOCK __SPRT_SC_MONOTONIC_CLOCK
#endif
#ifdef __SPRT_SC_READER_WRITER_LOCKS
#define SC_READER_WRITER_LOCKS __SPRT_SC_READER_WRITER_LOCKS
#endif
#ifdef __SPRT_SC_SPIN_LOCKS
#define SC_SPIN_LOCKS __SPRT_SC_SPIN_LOCKS
#endif
#ifdef __SPRT_SC_REGEXP
#define SC_REGEXP __SPRT_SC_REGEXP
#endif
#ifdef __SPRT_SC_SHELL
#define SC_SHELL __SPRT_SC_SHELL
#endif
#ifdef __SPRT_SC_SPAWN
#define SC_SPAWN __SPRT_SC_SPAWN
#endif
#ifdef __SPRT_SC_SPORADIC_SERVER
#define SC_SPORADIC_SERVER __SPRT_SC_SPORADIC_SERVER
#endif
#ifdef __SPRT_SC_THREAD_SPORADIC_SERVER
#define SC_THREAD_SPORADIC_SERVER __SPRT_SC_THREAD_SPORADIC_SERVER
#endif
#ifdef __SPRT_SC_TIMEOUTS
#define SC_TIMEOUTS __SPRT_SC_TIMEOUTS
#endif
#ifdef __SPRT_SC_TYPED_MEMORY_OBJECTS
#define SC_TYPED_MEMORY_OBJECTS __SPRT_SC_TYPED_MEMORY_OBJECTS
#endif
#ifdef __SPRT_SC_2_PBS
#define SC_2_PBS __SPRT_SC_2_PBS
#endif
#ifdef __SPRT_SC_2_PBS_ACCOUNTING
#define SC_2_PBS_ACCOUNTING __SPRT_SC_2_PBS_ACCOUNTING
#endif
#ifdef __SPRT_SC_2_PBS_LOCATE
#define SC_2_PBS_LOCATE __SPRT_SC_2_PBS_LOCATE
#endif
#ifdef __SPRT_SC_2_PBS_MESSAGE
#define SC_2_PBS_MESSAGE __SPRT_SC_2_PBS_MESSAGE
#endif
#ifdef __SPRT_SC_2_PBS_TRACK
#define SC_2_PBS_TRACK __SPRT_SC_2_PBS_TRACK
#endif
#ifdef __SPRT_SC_SYMLOOP_MAX
#define SC_SYMLOOP_MAX __SPRT_SC_SYMLOOP_MAX
#endif
#ifdef __SPRT_SC_STREAMS
#define SC_STREAMS __SPRT_SC_STREAMS
#endif
#ifdef __SPRT_SC_2_PBS_CHECKPOINT
#define SC_2_PBS_CHECKPOINT __SPRT_SC_2_PBS_CHECKPOINT
#endif
#ifdef __SPRT_SC_V6_ILP32_OFF32
#define SC_V6_ILP32_OFF32 __SPRT_SC_V6_ILP32_OFF32
#endif
#ifdef __SPRT_SC_V6_ILP32_OFFBIG
#define SC_V6_ILP32_OFFBIG __SPRT_SC_V6_ILP32_OFFBIG
#endif
#ifdef __SPRT_SC_V6_LP64_OFF64
#define SC_V6_LP64_OFF64 __SPRT_SC_V6_LP64_OFF64
#endif
#ifdef __SPRT_SC_V6_LPBIG_OFFBIG
#define SC_V6_LPBIG_OFFBIG __SPRT_SC_V6_LPBIG_OFFBIG
#endif
#ifdef __SPRT_SC_HOST_NAME_MAX
#define SC_HOST_NAME_MAX __SPRT_SC_HOST_NAME_MAX
#endif
#ifdef __SPRT_SC_TRACE
#define SC_TRACE __SPRT_SC_TRACE
#endif
#ifdef __SPRT_SC_TRACE_EVENT_FILTER
#define SC_TRACE_EVENT_FILTER __SPRT_SC_TRACE_EVENT_FILTER
#endif
#ifdef __SPRT_SC_TRACE_INHERIT
#define SC_TRACE_INHERIT __SPRT_SC_TRACE_INHERIT
#endif
#ifdef __SPRT_SC_TRACE_LOG
#define SC_TRACE_LOG __SPRT_SC_TRACE_LOG
#endif
#ifdef __SPRT_SC_IPV6
#define SC_IPV6 __SPRT_SC_IPV6
#endif
#ifdef __SPRT_SC_RAW_SOCKETS
#define SC_RAW_SOCKETS __SPRT_SC_RAW_SOCKETS
#endif
#ifdef __SPRT_SC_V7_ILP32_OFF32
#define SC_V7_ILP32_OFF32 __SPRT_SC_V7_ILP32_OFF32
#endif
#ifdef __SPRT_SC_V7_ILP32_OFFBIG
#define SC_V7_ILP32_OFFBIG __SPRT_SC_V7_ILP32_OFFBIG
#endif
#ifdef __SPRT_SC_V7_LP64_OFF64
#define SC_V7_LP64_OFF64 __SPRT_SC_V7_LP64_OFF64
#endif
#ifdef __SPRT_SC_V7_LPBIG_OFFBIG
#define SC_V7_LPBIG_OFFBIG __SPRT_SC_V7_LPBIG_OFFBIG
#endif
#ifdef __SPRT_SC_SS_REPL_MAX
#define SC_SS_REPL_MAX __SPRT_SC_SS_REPL_MAX
#endif
#ifdef __SPRT_SC_TRACE_EVENT_NAME_MAX
#define SC_TRACE_EVENT_NAME_MAX __SPRT_SC_TRACE_EVENT_NAME_MAX
#endif
#ifdef __SPRT_SC_TRACE_NAME_MAX
#define SC_TRACE_NAME_MAX __SPRT_SC_TRACE_NAME_MAX
#endif
#ifdef __SPRT_SC_TRACE_SYS_MAX
#define SC_TRACE_SYS_MAX __SPRT_SC_TRACE_SYS_MAX
#endif
#ifdef __SPRT_SC_TRACE_USER_EVENT_MAX
#define SC_TRACE_USER_EVENT_MAX __SPRT_SC_TRACE_USER_EVENT_MAX
#endif
#ifdef __SPRT_SC_XOPEN_STREAMS
#define SC_XOPEN_STREAMS __SPRT_SC_XOPEN_STREAMS
#endif
#ifdef __SPRT_SC_THREAD_ROBUST_PRIO_INHERIT
#define SC_THREAD_ROBUST_PRIO_INHERIT __SPRT_SC_THREAD_ROBUST_PRIO_INHERIT
#endif
#ifdef __SPRT_SC_THREAD_ROBUST_PRIO_PROTECT
#define SC_THREAD_ROBUST_PRIO_PROTECT __SPRT_SC_THREAD_ROBUST_PRIO_PROTECT
#endif
#ifdef __SPRT_SC_MINSIGSTKSZ
#define SC_MINSIGSTKSZ __SPRT_SC_MINSIGSTKSZ
#endif
#ifdef __SPRT_SC_SIGSTKSZ
#define SC_SIGSTKSZ __SPRT_SC_SIGSTKSZ
#endif

#ifdef __SPRT_PC_LINK_MAX
#define PC_LINK_MAX __SPRT_PC_LINK_MAX
#endif
#ifdef __SPRT_PC_MAX_CANON
#define PC_MAX_CANON __SPRT_PC_MAX_CANON
#endif
#ifdef __SPRT_PC_MAX_INPUT
#define PC_MAX_INPUT __SPRT_PC_MAX_INPUT
#endif
#ifdef __SPRT_PC_NAME_MAX
#define PC_NAME_MAX __SPRT_PC_NAME_MAX
#endif
#ifdef __SPRT_PC_PATH_MAX
#define PC_PATH_MAX __SPRT_PC_PATH_MAX
#endif
#ifdef __SPRT_PC_PIPE_BUF
#define PC_PIPE_BUF __SPRT_PC_PIPE_BUF
#endif
#ifdef __SPRT_PC_CHOWN_RESTRICTED
#define PC_CHOWN_RESTRICTED __SPRT_PC_CHOWN_RESTRICTED
#endif
#ifdef __SPRT_PC_NO_TRUNC
#define PC_NO_TRUNC __SPRT_PC_NO_TRUNC
#endif
#ifdef __SPRT_PC_VDISABLE
#define PC_VDISABLE __SPRT_PC_VDISABLE
#endif
#ifdef __SPRT_PC_SYNC_IO
#define PC_SYNC_IO __SPRT_PC_SYNC_IO
#endif
#ifdef __SPRT_PC_ASYNC_IO
#define PC_ASYNC_IO __SPRT_PC_ASYNC_IO
#endif
#ifdef __SPRT_PC_PRIO_IO
#define PC_PRIO_IO __SPRT_PC_PRIO_IO
#endif
#ifdef __SPRT_PC_SOCK_MAXBUF
#define PC_SOCK_MAXBUF __SPRT_PC_SOCK_MAXBUF
#endif
#ifdef __SPRT_PC_FILESIZEBITS
#define PC_FILESIZEBITS __SPRT_PC_FILESIZEBITS
#endif
#ifdef __SPRT_PC_REC_INCR_XFER_SIZE
#define PC_REC_INCR_XFER_SIZE __SPRT_PC_REC_INCR_XFER_SIZE
#endif
#ifdef __SPRT_PC_REC_MAX_XFER_SIZE
#define PC_REC_MAX_XFER_SIZE __SPRT_PC_REC_MAX_XFER_SIZE
#endif
#ifdef __SPRT_PC_REC_MIN_XFER_SIZE
#define PC_REC_MIN_XFER_SIZE __SPRT_PC_REC_MIN_XFER_SIZE
#endif
#ifdef __SPRT_PC_REC_XFER_ALIGN
#define PC_REC_XFER_ALIGN __SPRT_PC_REC_XFER_ALIGN
#endif
#ifdef __SPRT_PC_ALLOC_SIZE_MIN
#define PC_ALLOC_SIZE_MIN __SPRT_PC_ALLOC_SIZE_MIN
#endif
#ifdef __SPRT_PC_SYMLINK_MAX
#define PC_SYMLINK_MAX __SPRT_PC_SYMLINK_MAX
#endif
#ifdef __SPRT_PC_2_SYMLINKS
#define PC_2_SYMLINKS __SPRT_PC_2_SYMLINKS
#endif


__SPRT_BEGIN_DECL

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(ssize_t) ssize_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(time_t) time_t;
typedef __SPRT_ID(uid_t) uid_t;
typedef __SPRT_ID(gid_t) gid_t;
typedef __SPRT_ID(pid_t) pid_t;
typedef __SPRT_ID(intptr_t) intptr_t;

SPRT_FORCEINLINE int access(const char *path, int __type) __SPRT_NOEXCEPT {
	return __sprt_access(path, __type);
}

SPRT_FORCEINLINE int eaccess(const char *path, int __type) __SPRT_NOEXCEPT {
	return __sprt_eaccess(path, __type);
}

SPRT_FORCEINLINE off_t lseek(int __fd, off_t __offset, int __whence) __SPRT_NOEXCEPT {
	return __sprt_lseek(__fd, __offset, __whence);
}

SPRT_FORCEINLINE int close(int __fd) { return __sprt_close(__fd); }

SPRT_FORCEINLINE ssize_t read(int __fd, void *__buf, size_t __nbytes) {
	return __sprt_read(__fd, __buf, __nbytes);
}

SPRT_FORCEINLINE ssize_t write(int __fd, const void *__buf, size_t __n) {
	return __sprt_write(__fd, __buf, __n);
}

SPRT_FORCEINLINE ssize_t pread(int __fd, void *__buf, size_t __count, off_t __offset) {
	return __sprt_pread(__fd, __buf, __count, __offset);
}

SPRT_FORCEINLINE ssize_t pwrite(int __fd, const void *__buf, size_t __count, off_t __offset) {
	return __sprt_pwrite(__fd, __buf, __count, __offset);
}

SPRT_FORCEINLINE unsigned int sleep(unsigned int __seconds) { return __sprt_sleep(__seconds); }

SPRT_FORCEINLINE int usleep(time_t __useconds) { return __sprt_usleep(__useconds); }

#if __SPRT_CONFIG_HAVE_UNISTD_CHOWN || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE int chown(const char *__file, uid_t __owner, gid_t __group) __SPRT_NOEXCEPT {
	return __sprt_chown(__file, __owner, __group);
}
#endif

SPRT_FORCEINLINE int fchown(int __fd, uid_t __owner, gid_t __group) __SPRT_NOEXCEPT {
	return __sprt_fchown(__fd, __owner, __group);
}

SPRT_FORCEINLINE int chdir(const char *path) __SPRT_NOEXCEPT { return __sprt_chdir(path); }

SPRT_FORCEINLINE int fchdir(int __fd) __SPRT_NOEXCEPT { return __sprt_fchdir(__fd); }

SPRT_FORCEINLINE char *getcwd(char *__buf, size_t __size) __SPRT_NOEXCEPT {
	return __sprt_getcwd(__buf, __size);
}

#if __SPRT_CONFIG_HAVE_UNISTD_DUP || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE int dup(int __fd) __SPRT_NOEXCEPT { return __sprt_dup(__fd); }

SPRT_FORCEINLINE int dup2(int __fd, int __fd2) __SPRT_NOEXCEPT { return __sprt_dup2(__fd, __fd2); }
#endif

#if __SPRT_CONFIG_HAVE_UNISTD_DUP3 || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE int dup3(int __fd, int __fd2, int __flags) __SPRT_NOEXCEPT {
	return __sprt_dup3(__fd, __fd2, __flags);
}
#endif

#if __SPRT_CONFIG_HAVE_UNISTD_EXEC || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

SPRT_FORCEINLINE int execve(const char *__path, char *const __argv[],
		char *const __envp[]) __SPRT_NOEXCEPT {
	return __sprt_execve(__path, __argv, __envp);
}

SPRT_FORCEINLINE int fexecve(int __fd, char *const __argv[], char *const __envp[]) __SPRT_NOEXCEPT {
	return __sprt_fexecve(__fd, __argv, __envp);
}

SPRT_FORCEINLINE int execv(const char *__path, char *const __argv[]) __SPRT_NOEXCEPT {
	return __sprt_execv(__path, __argv);
}

SPRT_FORCEINLINE int execvp(const char *__file, char *const __argv[]) __SPRT_NOEXCEPT {
	return __sprt_execvp(__file, __argv);
}

SPRT_FORCEINLINE int execvpe(const char *__file, char *const __argv[],
		char *const __envp[]) __SPRT_NOEXCEPT {
	return __sprt_execvpe(__file, __argv, __envp);
}


SPRT_FORCEINLINE int execle(const char *__path, const char *__arg, ...) __SPRT_NOEXCEPT {
	int argc;
	__sprt_va_list ap;
	__sprt_va_start(ap, __arg);
	for (argc = 1; __sprt_va_arg(ap, const char *); argc++);
	__sprt_va_end(ap);

	int i;
	char *argv[argc + 1];
	char **envp;
	__sprt_va_start(ap, __arg);
	argv[0] = (char *)__arg;
	for (i = 1; i <= argc; i++) { argv[i] = __sprt_va_arg(ap, char *); }
	envp = __sprt_va_arg(ap, char **);
	__sprt_va_end(ap);
	return __sprt_execve(__path, argv, envp);
}

SPRT_FORCEINLINE int execl(const char *__path, const char *__arg, ...) __SPRT_NOEXCEPT {
	int argc;
	__sprt_va_list ap;
	__sprt_va_start(ap, __arg);
	for (argc = 1; __sprt_va_arg(ap, const char *); argc++);
	__sprt_va_end(ap);

	int i;
	char *argv[argc + 1];
	__sprt_va_start(ap, __arg);
	argv[0] = (char *)__arg;
	for (i = 1; i < argc; i++) { argv[i] = __sprt_va_arg(ap, char *); }
	argv[i] = nullptr;
	__sprt_va_end(ap);
	return __sprt_execv(__path, argv);
}

SPRT_FORCEINLINE int execlp(const char *__file, const char *__arg, ...) __SPRT_NOEXCEPT {
	int argc;
	__sprt_va_list ap;
	__sprt_va_start(ap, __arg);
	for (argc = 1; __sprt_va_arg(ap, const char *); argc++);
	__sprt_va_end(ap);

	int i;
	char *argv[argc + 1];
	__sprt_va_start(ap, __arg);
	argv[0] = (char *)__arg;
	for (i = 1; i < argc; i++) { argv[i] = __sprt_va_arg(ap, char *); }
	argv[i] = nullptr;
	__sprt_va_end(ap);
	return __sprt_execvp(__file, argv);
}

#endif

#if __SPRT_CONFIG_HAVE_UNISTD_NICE || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE int nice(int __inc) __SPRT_NOEXCEPT { return __sprt_nice(__inc); }
#endif

SPRT_FORCEINLINE long int pathconf(const char *__path, int __name) __SPRT_NOEXCEPT {
	return __sprt_pathconf(__path, __name);
}

SPRT_FORCEINLINE long int fpathconf(int __fd, int __name) __SPRT_NOEXCEPT {
	return __sprt_fpathconf(__fd, __name);
}

SPRT_FORCEINLINE long int sysconf(int __name) __SPRT_NOEXCEPT { return __sprt_sysconf(__name); }

SPRT_FORCEINLINE pid_t getpid(void) __SPRT_NOEXCEPT { return __sprt_getpid(); }

#if __SPRT_CONFIG_HAVE_UNISTD_GETPPID || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE pid_t getppid(void) __SPRT_NOEXCEPT { return __sprt_getppid(); }
#endif

SPRT_FORCEINLINE uid_t getuid(void) __SPRT_NOEXCEPT { return __sprt_getuid(); }

SPRT_FORCEINLINE uid_t geteuid(void) __SPRT_NOEXCEPT { return __sprt_geteuid(); }

SPRT_FORCEINLINE gid_t getgid(void) __SPRT_NOEXCEPT { return __sprt_getgid(); }

SPRT_FORCEINLINE gid_t getegid(void) __SPRT_NOEXCEPT { return __sprt_getegid(); }

SPRT_FORCEINLINE int getresuid(uid_t *__ruid, uid_t *__euid, uid_t *__suid) __SPRT_NOEXCEPT {
	return __sprt_getresuid(__ruid, __euid, __suid);
}
SPRT_FORCEINLINE int getresgid(gid_t *__rgid, gid_t *__egid, gid_t *__sgid) __SPRT_NOEXCEPT {
	return __sprt_getresgid(__rgid, __egid, __sgid);
}

SPRT_FORCEINLINE int getgroups(int __size, gid_t __list[]) __SPRT_NOEXCEPT {
	return __sprt_getgroups(__size, __list);
}

#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE int setuid(uid_t __uid) __SPRT_NOEXCEPT { return __sprt_setuid(__uid); }
SPRT_FORCEINLINE int setreuid(uid_t __ruid, uid_t __euid) __SPRT_NOEXCEPT {
	return __sprt_setreuid(__ruid, __euid);
}
SPRT_FORCEINLINE int seteuid(uid_t __uid) __SPRT_NOEXCEPT { return __sprt_seteuid(__uid); }
SPRT_FORCEINLINE int setgid(gid_t __gid) __SPRT_NOEXCEPT { return __sprt_setgid(__gid); }
SPRT_FORCEINLINE int setregid(gid_t __rgid, gid_t __egid) __SPRT_NOEXCEPT {
	return __sprt_setregid(__rgid, __egid);
}
SPRT_FORCEINLINE int setegid(gid_t __gid) __SPRT_NOEXCEPT { return __sprt_setegid(__gid); }
SPRT_FORCEINLINE int setresuid(uid_t __ruid, uid_t __euid, uid_t __suid) __SPRT_NOEXCEPT {
	return __sprt_setresuid(__ruid, __euid, __suid);
}
SPRT_FORCEINLINE int setresgid(gid_t __rgid, gid_t __egid, gid_t __sgid) __SPRT_NOEXCEPT {
	return __sprt_setresgid(__rgid, __egid, __sgid);
}
#endif

#if __SPRT_CONFIG_HAVE_UNISTD_FORK || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE pid_t fork(void) __SPRT_NOEXCEPT { return __sprt_fork(); }
SPRT_FORCEINLINE pid_t vfork(void) __SPRT_NOEXCEPT { return __sprt_vfork(); }
#endif

SPRT_FORCEINLINE char *ttyname(int __fd) __SPRT_NOEXCEPT { return __sprt_ttyname(__fd); }
SPRT_FORCEINLINE int ttyname_r(int __fd, char *__buf, size_t __buflen) __SPRT_NOEXCEPT {
	return __sprt_ttyname_r(__fd, __buf, __buflen);
}
SPRT_FORCEINLINE int isatty(int __fd) __SPRT_NOEXCEPT { return __sprt_isatty(__fd); }

SPRT_FORCEINLINE int link(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return __sprt_link(__from, __to);
}
SPRT_FORCEINLINE int symlink(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return __sprt_symlink(__from, __to);
}
SPRT_FORCEINLINE ssize_t readlink(const char *__SPRT_RESTRICT __path, char *__SPRT_RESTRICT __buf,
		size_t __len) __SPRT_NOEXCEPT {
	return __sprt_readlink(__path, __buf, __len);
}
SPRT_FORCEINLINE int unlink(const char *__name) __SPRT_NOEXCEPT { return __sprt_unlink(__name); }
SPRT_FORCEINLINE int rmdir(const char *__path) __SPRT_NOEXCEPT { return __sprt_rmdir(__path); }
SPRT_FORCEINLINE char *getlogin(void) { return __sprt_getlogin(); }
SPRT_FORCEINLINE int getlogin_r(char *__name, size_t __name_len) {
	return __sprt_getlogin_r(__name, __name_len);
}

#if __SPRT_CONFIG_HAVE_UNISTD_SETLOGIN || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE int setlogin(const char *__name) __SPRT_NOEXCEPT {
	return __sprt_setlogin(__name);
}
#endif

SPRT_FORCEINLINE int gethostname(char *__name, size_t __len) __SPRT_NOEXCEPT {
	return __sprt_gethostname(__name, __len);
}

#if __SPRT_CONFIG_HAVE_UNISTD_SETHOSTNAME || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE int sethostname(const char *__name, size_t __len) __SPRT_NOEXCEPT {
	return __sprt_sethostname(__name, __len);
}
#endif

#if __SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE int getdomainname(char *__name, size_t __len) __SPRT_NOEXCEPT {
	return __sprt_getdomainname(__name, __len);
}
SPRT_FORCEINLINE int setdomainname(const char *__name, size_t __len) __SPRT_NOEXCEPT {
	return __sprt_setdomainname(__name, __len);
}
#endif

SPRT_FORCEINLINE int fsync(int __fd) { return __sprt_fsync(__fd); }
SPRT_FORCEINLINE void sync(void) __SPRT_NOEXCEPT { return __sprt_sync(); }
SPRT_FORCEINLINE int getpagesize(void) __SPRT_NOEXCEPT { return __sprt_getpagesize(); }
SPRT_FORCEINLINE int getdtablesize(void) __SPRT_NOEXCEPT { return __sprt_getdtablesize(); }
SPRT_FORCEINLINE int truncate(const char *__file, off_t length) __SPRT_NOEXCEPT {
	return __sprt_truncate(__file, length);
}
SPRT_FORCEINLINE int ftruncate(int __fd, off_t length) __SPRT_NOEXCEPT {
	return __sprt_ftruncate(__fd, length);
}

#if __SPRT_CONFIG_HAVE_UNISTD_BRK || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE int brk(void *__addr) __SPRT_NOEXCEPT { return __sprt_brk(__addr); }
SPRT_FORCEINLINE void *sbrk(intptr_t __delta) __SPRT_NOEXCEPT { return __sprt_sbrk(__delta); }
#endif

SPRT_FORCEINLINE int lockf(int __fd, int __cmd, off_t len) {
	return __sprt_lockf(__fd, __cmd, len);
}
SPRT_FORCEINLINE ssize_t copy_file_range(int __infd, off_t *__pinoff, int __outfd, off_t *__poutoff,
		size_t __length, unsigned int __flags) {
	return __sprt_copy_file_range(__infd, __pinoff, __outfd, __poutoff, __length, __flags);
}
SPRT_FORCEINLINE pid_t gettid(void) { return __sprt_gettid(); }
SPRT_FORCEINLINE int fdatasync(int __fildes) { return __sprt_fdatasync(__fildes); }

SPRT_FORCEINLINE void swab(const void *__SPRT_RESTRICT __from, void *__SPRT_RESTRICT __to,
		ssize_t __n) __SPRT_NOEXCEPT {
	return __sprt_swab(__from, __to, __n);
}
SPRT_FORCEINLINE int getentropy(void *__buffer, size_t __length) {
	return __sprt_getentropy(__buffer, __length);
}

SPRT_FORCEINLINE int symlinkat(const char *__old_path, int __new_dir_fd, const char *__new_path) {
	return __sprt_symlinkat(__old_path, __new_dir_fd, __new_path);
}

SPRT_FORCEINLINE ssize_t readlinkat(int __dir_fd, const char *__path, char *__buf,
		size_t __buf_size) {
	return __sprt_readlinkat(__dir_fd, __path, __buf, __buf_size);
}

SPRT_FORCEINLINE int fchownat(int __dir_fd, const char *__path, uid_t __owner, gid_t __group,
		int __flags) {
	return __sprt_fchownat(__dir_fd, __path, __owner, __group, __flags);
}

SPRT_FORCEINLINE int faccessat(int __dirfd, const char *__path, int __mode, int __flags) {
	return __sprt_faccessat(__dirfd, __path, __mode, __flags);
}

SPRT_FORCEINLINE int linkat(int __old_dir_fd, const char *__old_path, int __new_dir_fd,
		const char *__new_path, int __flags) {
	return __sprt_linkat(__old_dir_fd, __old_path, __new_dir_fd, __new_path, __flags);
}
SPRT_FORCEINLINE int unlinkat(int __dirfd, const char *__path, int __flags) {
	return __sprt_unlinkat(__dirfd, __path, __flags);
}

SPRT_FORCEINLINE long gethostid(void) { return __sprt_gethostid(); }

SPRT_FORCEINLINE int pipe(int fds[2]) { return __sprt_pipe(fds); }
SPRT_FORCEINLINE int pipe2(int fds[2], int flags) { return __sprt_pipe2(fds, flags); }

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_UNISTD_H_
