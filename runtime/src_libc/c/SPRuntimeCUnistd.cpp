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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_stdarg.h>

#include <sprt/runtime/hash.h>
#include <sprt/runtime/platform.h>
#include <sprt/runtime/log.h>

#include <limits.h>

#if SPRT_LINUX
#define _LARGEFILE64_SOURCE 1
#endif

#if SPRT_WINDOWS
#define NOMINMAX 1
#include <Windows.h>
#include <corecrt_io.h>
#include <direct.h>
#include <process.h>
#include <stdio.h>

#include "platform/windows/unistd.cc"
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>
#endif

#include <stdlib.h>

#include "private/SPRTFilename.h"
#include "private/SPRTSpecific.h"

#if SPRT_MACOS
#include <sys/random.h>
#include <pthread.h>
#endif

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"
#endif

static_assert(SEEK_SET == __SPRT_SEEK_SET);
static_assert(SEEK_CUR == __SPRT_SEEK_CUR);
static_assert(SEEK_END == __SPRT_SEEK_END);

static_assert(R_OK == __SPRT_R_OK);
static_assert(W_OK == __SPRT_W_OK);
static_assert(X_OK == __SPRT_X_OK);
static_assert(F_OK == __SPRT_F_OK);

static_assert(AT_FDCWD == __SPRT_AT_FDCWD);
static_assert(AT_SYMLINK_NOFOLLOW == __SPRT_AT_SYMLINK_NOFOLLOW);
static_assert(AT_SYMLINK_FOLLOW == __SPRT_AT_SYMLINK_FOLLOW);

#ifdef AT_NO_AUTOMOUNT
static_assert(AT_NO_AUTOMOUNT == __SPRT_AT_NO_AUTOMOUNT);
#endif

#ifdef AT_EMPTY_PATH
static_assert(AT_EMPTY_PATH == __SPRT_AT_EMPTY_PATH);
#endif

#ifdef AT_STATX_SYNC_TYPE
static_assert(AT_STATX_SYNC_TYPE == __SPRT_AT_STATX_SYNC_TYPE);
#endif

#ifdef AT_STATX_SYNC_AS_STAT
static_assert(AT_STATX_SYNC_AS_STAT == __SPRT_AT_STATX_SYNC_AS_STAT);
#endif

#ifdef AT_STATX_FORCE_SYNC
static_assert(AT_STATX_FORCE_SYNC == __SPRT_AT_STATX_FORCE_SYNC);
#endif

#ifdef AT_STATX_DONT_SYNC
static_assert(AT_STATX_DONT_SYNC == __SPRT_AT_STATX_DONT_SYNC);
#endif

#ifdef AT_RECURSIVE
static_assert(AT_RECURSIVE == __SPRT_AT_RECURSIVE);
#endif

#ifdef AT_EACCESS
static_assert(AT_EACCESS == __SPRT_AT_EACCESS);
#endif

#ifdef AT_REMOVEDIR
static_assert(AT_REMOVEDIR == __SPRT_AT_REMOVEDIR);
#endif

#ifdef AT_RENAME_NOREPLACE
static_assert(AT_RENAME_NOREPLACE == __SPRT_AT_RENAME_NOREPLACE);
#endif

#ifdef AT_RENAME_EXCHANGE
static_assert(AT_RENAME_EXCHANGE == __SPRT_AT_RENAME_EXCHANGE);
#endif

#ifdef AT_RENAME_WHITEOUT
static_assert(AT_RENAME_WHITEOUT == __SPRT_AT_RENAME_WHITEOUT);
#endif

#ifdef AT_HANDLE_FID
static_assert(AT_HANDLE_FID == __SPRT_AT_HANDLE_FID);
#endif

#ifdef AT_HANDLE_MNT_ID_UNIQUE
static_assert(AT_HANDLE_MNT_ID_UNIQUE == __SPRT_AT_HANDLE_MNT_ID_UNIQUE);
#endif

#ifdef _SC_ARG_MAX
static_assert(_SC_ARG_MAX == __SPRT_SC_ARG_MAX);
#endif

#ifdef _SC_CHILD_MAX
static_assert(_SC_CHILD_MAX == __SPRT_SC_CHILD_MAX);
#endif

#ifdef _SC_CLK_TCK
static_assert(_SC_CLK_TCK == __SPRT_SC_CLK_TCK);
#endif

#ifdef _SC_NGROUPS_MAX
static_assert(_SC_NGROUPS_MAX == __SPRT_SC_NGROUPS_MAX);
#endif

#ifdef _SC_OPEN_MAX
static_assert(_SC_OPEN_MAX == __SPRT_SC_OPEN_MAX);
#endif

#ifdef _SC_STREAM_MAX
static_assert(_SC_STREAM_MAX == __SPRT_SC_STREAM_MAX);
#endif

#ifdef _SC_TZNAME_MAX
static_assert(_SC_TZNAME_MAX == __SPRT_SC_TZNAME_MAX);
#endif

#ifdef _SC_JOB_CONTROL
static_assert(_SC_JOB_CONTROL == __SPRT_SC_JOB_CONTROL);
#endif

#ifdef _SC_SAVED_IDS
static_assert(_SC_SAVED_IDS == __SPRT_SC_SAVED_IDS);
#endif

#ifdef _SC_REALTIME_SIGNALS
static_assert(_SC_REALTIME_SIGNALS == __SPRT_SC_REALTIME_SIGNALS);
#endif

#ifdef _SC_PRIORITY_SCHEDULING
static_assert(_SC_PRIORITY_SCHEDULING == __SPRT_SC_PRIORITY_SCHEDULING);
#endif

#ifdef _SC_TIMERS
static_assert(_SC_TIMERS == __SPRT_SC_TIMERS);
#endif

#ifdef _SC_ASYNCHRONOUS_IO
static_assert(_SC_ASYNCHRONOUS_IO == __SPRT_SC_ASYNCHRONOUS_IO);
#endif

#ifdef _SC_PRIORITIZED_IO
static_assert(_SC_PRIORITIZED_IO == __SPRT_SC_PRIORITIZED_IO);
#endif

#ifdef _SC_SYNCHRONIZED_IO
static_assert(_SC_SYNCHRONIZED_IO == __SPRT_SC_SYNCHRONIZED_IO);
#endif

#ifdef _SC_FSYNC
static_assert(_SC_FSYNC == __SPRT_SC_FSYNC);
#endif

#ifdef _SC_MAPPED_FILES
static_assert(_SC_MAPPED_FILES == __SPRT_SC_MAPPED_FILES);
#endif

#ifdef _SC_MEMLOCK
static_assert(_SC_MEMLOCK == __SPRT_SC_MEMLOCK);
#endif

#ifdef _SC_MEMLOCK_RANGE
static_assert(_SC_MEMLOCK_RANGE == __SPRT_SC_MEMLOCK_RANGE);
#endif

#ifdef _SC_MEMORY_PROTECTION
static_assert(_SC_MEMORY_PROTECTION == __SPRT_SC_MEMORY_PROTECTION);
#endif

#ifdef _SC_MESSAGE_PASSING
static_assert(_SC_MESSAGE_PASSING == __SPRT_SC_MESSAGE_PASSING);
#endif

#ifdef _SC_SEMAPHORES
static_assert(_SC_SEMAPHORES == __SPRT_SC_SEMAPHORES);
#endif

#ifdef _SC_SHARED_MEMORY_OBJECTS
static_assert(_SC_SHARED_MEMORY_OBJECTS == __SPRT_SC_SHARED_MEMORY_OBJECTS);
#endif

#ifdef _SC_AIO_LISTIO_MAX
static_assert(_SC_AIO_LISTIO_MAX == __SPRT_SC_AIO_LISTIO_MAX);
#endif

#ifdef _SC_AIO_MAX
static_assert(_SC_AIO_MAX == __SPRT_SC_AIO_MAX);
#endif

#ifdef _SC_AIO_PRIO_DELTA_MAX
static_assert(_SC_AIO_PRIO_DELTA_MAX == __SPRT_SC_AIO_PRIO_DELTA_MAX);
#endif

#ifdef _SC_DELAYTIMER_MAX
static_assert(_SC_DELAYTIMER_MAX == __SPRT_SC_DELAYTIMER_MAX);
#endif

#ifdef _SC_MQ_OPEN_MAX
static_assert(_SC_MQ_OPEN_MAX == __SPRT_SC_MQ_OPEN_MAX);
#endif

#ifdef _SC_MQ_PRIO_MAX
static_assert(_SC_MQ_PRIO_MAX == __SPRT_SC_MQ_PRIO_MAX);
#endif

#ifdef _SC_VERSION
static_assert(_SC_VERSION == __SPRT_SC_VERSION);
#endif

#ifdef _SC_PAGE_SIZE
static_assert(_SC_PAGE_SIZE == __SPRT_SC_PAGE_SIZE);
#endif

#ifdef _SC_PAGESIZE
static_assert(_SC_PAGESIZE == __SPRT_SC_PAGESIZE);
#endif

#ifdef _SC_RTSIG_MAX
static_assert(_SC_RTSIG_MAX == __SPRT_SC_RTSIG_MAX);
#endif

#ifdef _SC_SEM_NSEMS_MAX
static_assert(_SC_SEM_NSEMS_MAX == __SPRT_SC_SEM_NSEMS_MAX);
#endif

#ifdef _SC_SEM_VALUE_MAX
static_assert(_SC_SEM_VALUE_MAX == __SPRT_SC_SEM_VALUE_MAX);
#endif

#ifdef _SC_SIGQUEUE_MAX
static_assert(_SC_SIGQUEUE_MAX == __SPRT_SC_SIGQUEUE_MAX);
#endif

#ifdef _SC_TIMER_MAX
static_assert(_SC_TIMER_MAX == __SPRT_SC_TIMER_MAX);
#endif

#ifdef _SC_BC_BASE_MAX
static_assert(_SC_BC_BASE_MAX == __SPRT_SC_BC_BASE_MAX);
#endif

#ifdef _SC_BC_DIM_MAX
static_assert(_SC_BC_DIM_MAX == __SPRT_SC_BC_DIM_MAX);
#endif

#ifdef _SC_BC_SCALE_MAX
static_assert(_SC_BC_SCALE_MAX == __SPRT_SC_BC_SCALE_MAX);
#endif

#ifdef _SC_BC_STRING_MAX
static_assert(_SC_BC_STRING_MAX == __SPRT_SC_BC_STRING_MAX);
#endif

#ifdef _SC_COLL_WEIGHTS_MAX
static_assert(_SC_COLL_WEIGHTS_MAX == __SPRT_SC_COLL_WEIGHTS_MAX);
#endif

#ifdef _SC_EXPR_NEST_MAX
static_assert(_SC_EXPR_NEST_MAX == __SPRT_SC_EXPR_NEST_MAX);
#endif

#ifdef _SC_LINE_MAX
static_assert(_SC_LINE_MAX == __SPRT_SC_LINE_MAX);
#endif

#ifdef _SC_RE_DUP_MAX
static_assert(_SC_RE_DUP_MAX == __SPRT_SC_RE_DUP_MAX);
#endif

#ifdef _SC_2_VERSION
static_assert(_SC_2_VERSION == __SPRT_SC_2_VERSION);
#endif

#ifdef _SC_2_C_BIND
static_assert(_SC_2_C_BIND == __SPRT_SC_2_C_BIND);
#endif

#ifdef _SC_2_C_DEV
static_assert(_SC_2_C_DEV == __SPRT_SC_2_C_DEV);
#endif

#ifdef _SC_2_FORT_DEV
static_assert(_SC_2_FORT_DEV == __SPRT_SC_2_FORT_DEV);
#endif

#ifdef _SC_2_FORT_RUN
static_assert(_SC_2_FORT_RUN == __SPRT_SC_2_FORT_RUN);
#endif

#ifdef _SC_2_SW_DEV
static_assert(_SC_2_SW_DEV == __SPRT_SC_2_SW_DEV);
#endif

#ifdef _SC_2_LOCALEDEF
static_assert(_SC_2_LOCALEDEF == __SPRT_SC_2_LOCALEDEF);
#endif

#ifdef _SC_UIO_MAXIOV
static_assert(_SC_UIO_MAXIOV == __SPRT_SC_UIO_MAXIOV);
#endif

#ifdef _SC_IOV_MAX
static_assert(_SC_IOV_MAX == __SPRT_SC_IOV_MAX);
#endif

#ifdef _SC_THREADS
static_assert(_SC_THREADS == __SPRT_SC_THREADS);
#endif

#ifdef _SC_THREAD_SAFE_FUNCTIONS
static_assert(_SC_THREAD_SAFE_FUNCTIONS == __SPRT_SC_THREAD_SAFE_FUNCTIONS);
#endif

#ifdef _SC_GETGR_R_SIZE_MAX
static_assert(_SC_GETGR_R_SIZE_MAX == __SPRT_SC_GETGR_R_SIZE_MAX);
#endif

#ifdef _SC_GETPW_R_SIZE_MAX
static_assert(_SC_GETPW_R_SIZE_MAX == __SPRT_SC_GETPW_R_SIZE_MAX);
#endif

#ifdef _SC_LOGIN_NAME_MAX
static_assert(_SC_LOGIN_NAME_MAX == __SPRT_SC_LOGIN_NAME_MAX);
#endif

#ifdef _SC_TTY_NAME_MAX
static_assert(_SC_TTY_NAME_MAX == __SPRT_SC_TTY_NAME_MAX);
#endif

#ifdef _SC_THREAD_DESTRUCTOR_ITERATIONS
static_assert(_SC_THREAD_DESTRUCTOR_ITERATIONS == __SPRT_SC_THREAD_DESTRUCTOR_ITERATIONS);
#endif

#ifdef _SC_THREAD_KEYS_MAX
static_assert(_SC_THREAD_KEYS_MAX == __SPRT_SC_THREAD_KEYS_MAX);
#endif

#ifdef _SC_THREAD_STACK_MIN
static_assert(_SC_THREAD_STACK_MIN == __SPRT_SC_THREAD_STACK_MIN);
#endif

#ifdef _SC_THREAD_THREADS_MAX
static_assert(_SC_THREAD_THREADS_MAX == __SPRT_SC_THREAD_THREADS_MAX);
#endif

#ifdef _SC_THREAD_ATTR_STACKADDR
static_assert(_SC_THREAD_ATTR_STACKADDR == __SPRT_SC_THREAD_ATTR_STACKADDR);
#endif

#ifdef _SC_THREAD_ATTR_STACKSIZE
static_assert(_SC_THREAD_ATTR_STACKSIZE == __SPRT_SC_THREAD_ATTR_STACKSIZE);
#endif

#ifdef _SC_THREAD_PRIORITY_SCHEDULING
static_assert(_SC_THREAD_PRIORITY_SCHEDULING == __SPRT_SC_THREAD_PRIORITY_SCHEDULING);
#endif

#ifdef _SC_THREAD_PRIO_INHERIT
static_assert(_SC_THREAD_PRIO_INHERIT == __SPRT_SC_THREAD_PRIO_INHERIT);
#endif

#ifdef _SC_THREAD_PRIO_PROTECT
static_assert(_SC_THREAD_PRIO_PROTECT == __SPRT_SC_THREAD_PRIO_PROTECT);
#endif

#ifdef _SC_THREAD_PROCESS_SHARED
static_assert(_SC_THREAD_PROCESS_SHARED == __SPRT_SC_THREAD_PROCESS_SHARED);
#endif

#ifdef _SC_NPROCESSORS_CONF
static_assert(_SC_NPROCESSORS_CONF == __SPRT_SC_NPROCESSORS_CONF);
#endif

#ifdef _SC_NPROCESSORS_ONLN
static_assert(_SC_NPROCESSORS_ONLN == __SPRT_SC_NPROCESSORS_ONLN);
#endif

#ifdef _SC_PHYS_PAGES
static_assert(_SC_PHYS_PAGES == __SPRT_SC_PHYS_PAGES);
#endif

#ifdef _SC_AVPHYS_PAGES
static_assert(_SC_AVPHYS_PAGES == __SPRT_SC_AVPHYS_PAGES);
#endif

#ifdef _SC_ATEXIT_MAX
static_assert(_SC_ATEXIT_MAX == __SPRT_SC_ATEXIT_MAX);
#endif

#ifdef _SC_PASS_MAX
static_assert(_SC_PASS_MAX == __SPRT_SC_PASS_MAX);
#endif

#ifdef _SC_XOPEN_VERSION
static_assert(_SC_XOPEN_VERSION == __SPRT_SC_XOPEN_VERSION);
#endif

#ifdef _SC_XOPEN_XCU_VERSION
static_assert(_SC_XOPEN_XCU_VERSION == __SPRT_SC_XOPEN_XCU_VERSION);
#endif

#ifdef _SC_XOPEN_UNIX
static_assert(_SC_XOPEN_UNIX == __SPRT_SC_XOPEN_UNIX);
#endif

#ifdef _SC_XOPEN_CRYPT
static_assert(_SC_XOPEN_CRYPT == __SPRT_SC_XOPEN_CRYPT);
#endif

#ifdef _SC_XOPEN_ENH_I18N
static_assert(_SC_XOPEN_ENH_I18N == __SPRT_SC_XOPEN_ENH_I18N);
#endif

#ifdef _SC_XOPEN_SHM
static_assert(_SC_XOPEN_SHM == __SPRT_SC_XOPEN_SHM);
#endif

#ifdef _SC_2_CHAR_TERM
static_assert(_SC_2_CHAR_TERM == __SPRT_SC_2_CHAR_TERM);
#endif

#ifdef _SC_2_UPE
static_assert(_SC_2_UPE == __SPRT_SC_2_UPE);
#endif

#ifdef _SC_XOPEN_XPG2
static_assert(_SC_XOPEN_XPG2 == __SPRT_SC_XOPEN_XPG2);
#endif

#ifdef _SC_XOPEN_XPG3
static_assert(_SC_XOPEN_XPG3 == __SPRT_SC_XOPEN_XPG3);
#endif

#ifdef _SC_XOPEN_XPG4
static_assert(_SC_XOPEN_XPG4 == __SPRT_SC_XOPEN_XPG4);
#endif

#ifdef _SC_NZERO
static_assert(_SC_NZERO == __SPRT_SC_NZERO);
#endif

#ifdef _SC_XBS5_ILP32_OFF32
static_assert(_SC_XBS5_ILP32_OFF32 == __SPRT_SC_XBS5_ILP32_OFF32);
#endif

#ifdef _SC_XBS5_ILP32_OFFBIG
static_assert(_SC_XBS5_ILP32_OFFBIG == __SPRT_SC_XBS5_ILP32_OFFBIG);
#endif

#ifdef _SC_XBS5_LP64_OFF64
static_assert(_SC_XBS5_LP64_OFF64 == __SPRT_SC_XBS5_LP64_OFF64);
#endif

#ifdef _SC_XBS5_LPBIG_OFFBIG
static_assert(_SC_XBS5_LPBIG_OFFBIG == __SPRT_SC_XBS5_LPBIG_OFFBIG);
#endif

#ifdef _SC_XOPEN_LEGACY
static_assert(_SC_XOPEN_LEGACY == __SPRT_SC_XOPEN_LEGACY);
#endif

#ifdef _SC_XOPEN_REALTIME
static_assert(_SC_XOPEN_REALTIME == __SPRT_SC_XOPEN_REALTIME);
#endif

#ifdef _SC_XOPEN_REALTIME_THREADS
static_assert(_SC_XOPEN_REALTIME_THREADS == __SPRT_SC_XOPEN_REALTIME_THREADS);
#endif

#ifdef _SC_ADVISORY_INFO
static_assert(_SC_ADVISORY_INFO == __SPRT_SC_ADVISORY_INFO);
#endif

#ifdef _SC_BARRIERS
static_assert(_SC_BARRIERS == __SPRT_SC_BARRIERS);
#endif

#ifdef _SC_CLOCK_SELECTION
static_assert(_SC_CLOCK_SELECTION == __SPRT_SC_CLOCK_SELECTION);
#endif

#ifdef _SC_CPUTIME
static_assert(_SC_CPUTIME == __SPRT_SC_CPUTIME);
#endif

#ifdef _SC_THREAD_CPUTIME
static_assert(_SC_THREAD_CPUTIME == __SPRT_SC_THREAD_CPUTIME);
#endif

#ifdef _SC_MONOTONIC_CLOCK
static_assert(_SC_MONOTONIC_CLOCK == __SPRT_SC_MONOTONIC_CLOCK);
#endif

#ifdef _SC_READER_WRITER_LOCKS
static_assert(_SC_READER_WRITER_LOCKS == __SPRT_SC_READER_WRITER_LOCKS);
#endif

#ifdef _SC_SPIN_LOCKS
static_assert(_SC_SPIN_LOCKS == __SPRT_SC_SPIN_LOCKS);
#endif

#ifdef _SC_REGEXP
static_assert(_SC_REGEXP == __SPRT_SC_REGEXP);
#endif

#ifdef _SC_SHELL
static_assert(_SC_SHELL == __SPRT_SC_SHELL);
#endif

#ifdef _SC_SPAWN
static_assert(_SC_SPAWN == __SPRT_SC_SPAWN);
#endif

#ifdef _SC_SPORADIC_SERVER
static_assert(_SC_SPORADIC_SERVER == __SPRT_SC_SPORADIC_SERVER);
#endif

#ifdef _SC_THREAD_SPORADIC_SERVER
static_assert(_SC_THREAD_SPORADIC_SERVER == __SPRT_SC_THREAD_SPORADIC_SERVER);
#endif

#ifdef _SC_TIMEOUTS
static_assert(_SC_TIMEOUTS == __SPRT_SC_TIMEOUTS);
#endif

#ifdef _SC_TYPED_MEMORY_OBJECTS
static_assert(_SC_TYPED_MEMORY_OBJECTS == __SPRT_SC_TYPED_MEMORY_OBJECTS);
#endif

#ifdef _SC_2_PBS
static_assert(_SC_2_PBS == __SPRT_SC_2_PBS);
#endif

#ifdef _SC_2_PBS_ACCOUNTING
static_assert(_SC_2_PBS_ACCOUNTING == __SPRT_SC_2_PBS_ACCOUNTING);
#endif

#ifdef _SC_2_PBS_LOCATE
static_assert(_SC_2_PBS_LOCATE == __SPRT_SC_2_PBS_LOCATE);
#endif

#ifdef _SC_2_PBS_MESSAGE
static_assert(_SC_2_PBS_MESSAGE == __SPRT_SC_2_PBS_MESSAGE);
#endif

#ifdef _SC_2_PBS_TRACK
static_assert(_SC_2_PBS_TRACK == __SPRT_SC_2_PBS_TRACK);
#endif

#ifdef _SC_SYMLOOP_MAX
static_assert(_SC_SYMLOOP_MAX == __SPRT_SC_SYMLOOP_MAX);
#endif

#ifdef _SC_STREAMS
static_assert(_SC_STREAMS == __SPRT_SC_STREAMS);
#endif

#ifdef _SC_2_PBS_CHECKPOINT
static_assert(_SC_2_PBS_CHECKPOINT == __SPRT_SC_2_PBS_CHECKPOINT);
#endif

#ifdef _SC_V6_ILP32_OFF32
static_assert(_SC_V6_ILP32_OFF32 == __SPRT_SC_V6_ILP32_OFF32);
#endif

#ifdef _SC_V6_ILP32_OFFBIG
static_assert(_SC_V6_ILP32_OFFBIG == __SPRT_SC_V6_ILP32_OFFBIG);
#endif

#ifdef _SC_V6_LP64_OFF64
static_assert(_SC_V6_LP64_OFF64 == __SPRT_SC_V6_LP64_OFF64);
#endif

#ifdef _SC_V6_LPBIG_OFFBIG
static_assert(_SC_V6_LPBIG_OFFBIG == __SPRT_SC_V6_LPBIG_OFFBIG);
#endif

#ifdef _SC_HOST_NAME_MAX
static_assert(_SC_HOST_NAME_MAX == __SPRT_SC_HOST_NAME_MAX);
#endif

#ifdef _SC_TRACE
static_assert(_SC_TRACE == __SPRT_SC_TRACE);
#endif

#ifdef _SC_TRACE_EVENT_FILTER
static_assert(_SC_TRACE_EVENT_FILTER == __SPRT_SC_TRACE_EVENT_FILTER);
#endif

#ifdef _SC_TRACE_INHERIT
static_assert(_SC_TRACE_INHERIT == __SPRT_SC_TRACE_INHERIT);
#endif

#ifdef _SC_TRACE_LOG
static_assert(_SC_TRACE_LOG == __SPRT_SC_TRACE_LOG);
#endif

#ifdef _SC_IPV6
static_assert(_SC_IPV6 == __SPRT_SC_IPV6);
#endif

#ifdef _SC_RAW_SOCKETS
static_assert(_SC_RAW_SOCKETS == __SPRT_SC_RAW_SOCKETS);
#endif

#ifdef _SC_V7_ILP32_OFF32
static_assert(_SC_V7_ILP32_OFF32 == __SPRT_SC_V7_ILP32_OFF32);
#endif

#ifdef _SC_V7_ILP32_OFFBIG
static_assert(_SC_V7_ILP32_OFFBIG == __SPRT_SC_V7_ILP32_OFFBIG);
#endif

#ifdef _SC_V7_LP64_OFF64
static_assert(_SC_V7_LP64_OFF64 == __SPRT_SC_V7_LP64_OFF64);
#endif

#ifdef _SC_V7_LPBIG_OFFBIG
static_assert(_SC_V7_LPBIG_OFFBIG == __SPRT_SC_V7_LPBIG_OFFBIG);
#endif

#ifdef _SC_SS_REPL_MAX
static_assert(_SC_SS_REPL_MAX == __SPRT_SC_SS_REPL_MAX);
#endif

#ifdef _SC_TRACE_EVENT_NAME_MAX
static_assert(_SC_TRACE_EVENT_NAME_MAX == __SPRT_SC_TRACE_EVENT_NAME_MAX);
#endif

#ifdef _SC_TRACE_NAME_MAX
static_assert(_SC_TRACE_NAME_MAX == __SPRT_SC_TRACE_NAME_MAX);
#endif

#ifdef _SC_TRACE_SYS_MAX
static_assert(_SC_TRACE_SYS_MAX == __SPRT_SC_TRACE_SYS_MAX);
#endif

#ifdef _SC_TRACE_USER_EVENT_MAX
static_assert(_SC_TRACE_USER_EVENT_MAX == __SPRT_SC_TRACE_USER_EVENT_MAX);
#endif

#ifdef _SC_XOPEN_STREAMS
static_assert(_SC_XOPEN_STREAMS == __SPRT_SC_XOPEN_STREAMS);
#endif

#ifdef _SC_THREAD_ROBUST_PRIO_INHERIT
static_assert(_SC_THREAD_ROBUST_PRIO_INHERIT == __SPRT_SC_THREAD_ROBUST_PRIO_INHERIT);
#endif

#ifdef _SC_THREAD_ROBUST_PRIO_PROTECT
static_assert(_SC_THREAD_ROBUST_PRIO_PROTECT == __SPRT_SC_THREAD_ROBUST_PRIO_PROTECT);
#endif

#ifdef _SC_MINSIGSTKSZ
static_assert(_SC_MINSIGSTKSZ == __SPRT_SC_MINSIGSTKSZ);
#endif

#ifdef _SC_SIGSTKSZ
static_assert(_SC_SIGSTKSZ == __SPRT_SC_SIGSTKSZ);
#endif

#ifdef _PC_LINK_MAX
static_assert(_PC_LINK_MAX == __SPRT_PC_LINK_MAX);
#endif

#ifdef _PC_MAX_CANON
static_assert(_PC_MAX_CANON == __SPRT_PC_MAX_CANON);
#endif

#ifdef _PC_MAX_INPUT
static_assert(_PC_MAX_INPUT == __SPRT_PC_MAX_INPUT);
#endif

#ifdef _PC_NAME_MAX
static_assert(_PC_NAME_MAX == __SPRT_PC_NAME_MAX);
#endif

#ifdef _PC_PATH_MAX
static_assert(_PC_PATH_MAX == __SPRT_PC_PATH_MAX);
#endif

#ifdef _PC_PIPE_BUF
static_assert(_PC_PIPE_BUF == __SPRT_PC_PIPE_BUF);
#endif

#ifdef _PC_CHOWN_RESTRICTED
static_assert(_PC_CHOWN_RESTRICTED == __SPRT_PC_CHOWN_RESTRICTED);
#endif

#ifdef _PC_NO_TRUNC
static_assert(_PC_NO_TRUNC == __SPRT_PC_NO_TRUNC);
#endif

#ifdef _PC_VDISABLE
static_assert(_PC_VDISABLE == __SPRT_PC_VDISABLE);
#endif

#ifdef _PC_SYNC_IO
static_assert(_PC_SYNC_IO == __SPRT_PC_SYNC_IO);
#endif

#ifdef _PC_ASYNC_IO
static_assert(_PC_ASYNC_IO == __SPRT_PC_ASYNC_IO);
#endif

#ifdef _PC_PRIO_IO
static_assert(_PC_PRIO_IO == __SPRT_PC_PRIO_IO);
#endif

#ifdef _PC_SOCK_MAXBUF
static_assert(_PC_SOCK_MAXBUF == __SPRT_PC_SOCK_MAXBUF);
#endif

#ifdef _PC_FILESIZEBITS
static_assert(_PC_FILESIZEBITS == __SPRT_PC_FILESIZEBITS);
#endif

#ifdef _PC_REC_INCR_XFER_SIZE
static_assert(_PC_REC_INCR_XFER_SIZE == __SPRT_PC_REC_INCR_XFER_SIZE);
#endif

#ifdef _PC_REC_MAX_XFER_SIZE
static_assert(_PC_REC_MAX_XFER_SIZE == __SPRT_PC_REC_MAX_XFER_SIZE);
#endif

#ifdef _PC_REC_MIN_XFER_SIZE
static_assert(_PC_REC_MIN_XFER_SIZE == __SPRT_PC_REC_MIN_XFER_SIZE);
#endif

#ifdef _PC_REC_XFER_ALIGN
static_assert(_PC_REC_XFER_ALIGN == __SPRT_PC_REC_XFER_ALIGN);
#endif

#ifdef _PC_ALLOC_SIZE_MIN
static_assert(_PC_ALLOC_SIZE_MIN == __SPRT_PC_ALLOC_SIZE_MIN);
#endif

#ifdef _PC_SYMLINK_MAX
static_assert(_PC_SYMLINK_MAX == __SPRT_PC_SYMLINK_MAX);
#endif

#ifdef _PC_2_SYMLINKS
static_assert(_PC_2_SYMLINKS == __SPRT_PC_2_SYMLINKS);
#endif


namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(access)(const char *path, int __type) __SPRT_NOEXCEPT {
	return access(path, __type);
}

__SPRT_C_FUNC int __SPRT_ID(eaccess)(const char *path, int __type) __SPRT_NOEXCEPT {
#if SPRT_ANDROID || SPRT_MACOS
	return ::faccessat(-1, path, __type, __SPRT_AT_EACCESS);
#else
	return eaccess(path, __type);
#endif
}

__SPRT_C_FUNC __SPRT_ID(off_t)
		__SPRT_ID(lseek)(int __fd, __SPRT_ID(off_t) __offset, int __whence) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return ::_lseeki64(__fd, __offset, __whence);
#elif SPRT_MACOS
	return ::lseek(__fd, __offset, __whence);
#else
	return ::lseek64(__fd, __offset, __whence);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(close)(int __fd) {
#if SPRT_WINDOWS
	if (__fd < 0) {
		__sprt_errno = EBADF;
		return -1;
	}
	return ::_close(__fd);
#else
	return ::close(__fd);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(read)(int __fd, void *__buf, __SPRT_ID(size_t) __nbytes) {
	return read(__fd, __buf, __nbytes);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(write)(int __fd, const void *__buf, __SPRT_ID(size_t) __n) {

	return write(__fd, __buf, __n);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		pread)(int __fd, void *__buf, __SPRT_ID(size_t) __count, __SPRT_ID(off_t) __offset) {
#if SPRT_MACOS
	return pread(__fd, __buf, __count, __offset);
#else
	return pread64(__fd, __buf, __count, __offset);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		pwrite)(int __fd, const void *__buf, __SPRT_ID(size_t) __count, __SPRT_ID(off_t) __offset) {
#if SPRT_MACOS
	return pwrite(__fd, __buf, __count, __offset);
#else
	return pwrite64(__fd, __buf, __count, __offset);
#endif
}


__SPRT_C_FUNC unsigned int __SPRT_ID(sleep)(unsigned int __seconds) {
#if SPRT_WINDOWS
	::Sleep(__seconds * 1'000);
	return 0;
#else
	return ::sleep(__seconds);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(usleep)(__SPRT_ID(time_t) __useconds) { return usleep(__useconds); }

__SPRT_C_FUNC int __SPRT_ID(chown)(const char *__file, __SPRT_ID(uid_t) __owner,
		__SPRT_ID(gid_t) __group) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_CHOWN
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_CHOWN)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return ::chown(target, __owner, __group);
	}, -1);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		fchown)(int __fd, __SPRT_ID(uid_t) __owner, __SPRT_ID(gid_t) __group) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_CHOWN
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_CHOWN)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::fchown(__fd, __owner, __group);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(chdir)(const char *path) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		::_chdir(target);
		return 0;
	}, -1);
#else
	return ::chdir(path);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fchdir)(int __fd) __SPRT_NOEXCEPT { return fchdir(__fd); }

__SPRT_C_FUNC char *__SPRT_ID(getcwd)(char *__buf, __SPRT_ID(size_t) __size) __SPRT_NOEXCEPT {
	return getcwd(__buf, __size);
}

__SPRT_C_FUNC int __SPRT_ID(dup)(int __fd) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DUP
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DUP)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return ::_dup(__fd);
#else
	return ::dup(__fd);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(dup2)(int __fd, int __fd2) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DUP
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DUP)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return ::_dup2(__fd, __fd2);
#else
	return ::dup2(__fd, __fd2);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(dup3)(int __fd, int __fd2, int __flags) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DUP3
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DUP3)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::dup3(__fd, __fd2, __flags);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		execve)(const char *__path, char *const _argv[], char *const __envp[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return int(::_execve(target, _argv, __envp));
	}, -1);
#else
	return ::execve(__path, _argv, __envp);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		fexecve)(int __fd, char *const _argv[], char *const __envp[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_FEXEC
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_FEXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return fexecve(__fd, _argv, __envp);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execv)(const char *__path, char *const _argv[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return int(::_execv(target, _argv));
	}, -1);
#else
	return ::execv(__path, _argv);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execvp)(const char *__file, char *const _argv[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return int(::_execvp(target, _argv));
	}, -1);
#else
	return ::execvp(__file, _argv);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		execvpe)(const char *__file, char *const _argv[], char *const __envp[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return int(::_execvpe(target, _argv, __envp));
	}, -1);
#elif SPRT_MACOS
	return ::execve(__file, _argv, __envp);
#else
	return ::execvpe(__file, _argv, __envp);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execle)(const char *__path, const char *__arg, ...) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
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

#if SPRT_WINDOWS
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return int(::_execve(target, argv, envp));
	}, -1);
#else
	return ::execve(__path, argv, envp);
#endif
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execl)(const char *__path, const char *__arg, ...) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
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
	argv[i] = NULL;
	__sprt_va_end(ap);

#if SPRT_WINDOWS
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return int(::_execv(target, argv));
	}, -1);
#else
	return ::execv(__path, argv);
#endif
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execlp)(const char *__file, const char *__arg, ...) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else

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
	argv[i] = NULL;
	__sprt_va_end(ap);

#if SPRT_WINDOWS
	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return int(::_execvp(target, argv));
	}, -1);
#else
	return ::execvp(__file, argv);
#endif
#endif
}

__SPRT_C_FUNC int __SPRT_ID(nice)(int __inc) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_NICE
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_NICE)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::nice(__inc);
#endif
}

__SPRT_C_FUNC long int __SPRT_ID(pathconf)(const char *__path, int __name) __SPRT_NOEXCEPT {
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return (long int)pathconf(target, __name);
	}, (long int)-1);
}

__SPRT_C_FUNC long int __SPRT_ID(fpathconf)(int __fd, int __name) __SPRT_NOEXCEPT {
	return fpathconf(__fd, __name);
}

__SPRT_C_FUNC long int __SPRT_ID(sysconf)(int __name) __SPRT_NOEXCEPT { return sysconf(__name); }

__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(getpid)(void) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return GetCurrentProcessId();
#else
	return ::getpid();
#endif
}

__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(getppid)(void) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_GETPPID
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_GETPPID)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return getppid();
#endif
}

__SPRT_C_FUNC __SPRT_ID(uid_t) __SPRT_ID(getuid)(void) __SPRT_NOEXCEPT { return getuid(); }

__SPRT_C_FUNC __SPRT_ID(uid_t) __SPRT_ID(geteuid)(void) __SPRT_NOEXCEPT { return geteuid(); }

__SPRT_C_FUNC __SPRT_ID(gid_t) __SPRT_ID(getgid)(void) __SPRT_NOEXCEPT { return getgid(); }

__SPRT_C_FUNC __SPRT_ID(gid_t) __SPRT_ID(getegid)(void) __SPRT_NOEXCEPT { return getegid(); }

__SPRT_C_FUNC int __SPRT_ID(getgroups)(int __size, __SPRT_ID(gid_t) __list[]) __SPRT_NOEXCEPT {
	return getgroups(__size, __list);
}

__SPRT_C_FUNC int __SPRT_ID(setuid)(__SPRT_ID(uid_t) __uid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setuid(__uid);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(
		setreuid)(__SPRT_ID(uid_t) __ruid, __SPRT_ID(uid_t) __euid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setreuid(__ruid, __euid);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(seteuid)(__SPRT_ID(uid_t) __uid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::seteuid(__uid);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(setgid)(__SPRT_ID(gid_t) __gid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setgid(__gid);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(
		setregid)(__SPRT_ID(gid_t) __rgid, __SPRT_ID(gid_t) __egid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setregid(__rgid, __egid);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(setegid)(__SPRT_ID(gid_t) __gid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setegid(__gid);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(getresuid)(__SPRT_ID(uid_t) * __ruid, __SPRT_ID(uid_t) * __euid,
		__SPRT_ID(uid_t) * __suid) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS || SPRT_MACOS
	auto id = getuid();
	if (__ruid) {
		*__ruid = id;
	}
	if (__euid) {
		*__euid = geteuid();
	}
	if (__suid) {
		*__suid = id;
	}
	return 0;
#else
	return ::getresuid(__ruid, __euid, __suid);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(getresgid)(__SPRT_ID(gid_t) * __rgid, __SPRT_ID(gid_t) * __egid,
		__SPRT_ID(gid_t) * __sgid) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS || SPRT_MACOS
	auto id = getgid();
	if (__rgid) {
		*__rgid = id;
	}
	if (__egid) {
		*__egid = getegid();
	}
	if (__sgid) {
		*__sgid = id;
	}
	return 0;
#else
	return ::getresgid(__rgid, __egid, __sgid);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(setresuid)(__SPRT_ID(uid_t) __ruid, __SPRT_ID(uid_t) __euid,
		__SPRT_ID(uid_t) __suid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
#if SPRT_MACOS
	return ::setreuid(__ruid, __euid);
#else
	return ::setresuid(__ruid, __euid, __suid);
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(setresgid)(__SPRT_ID(gid_t) __rgid, __SPRT_ID(gid_t) __egid,
		__SPRT_ID(gid_t) __sgid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
#if SPRT_MACOS
	return ::setregid(__rgid, __egid);
#else
	return ::setresgid(__rgid, __egid, __sgid);
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(fork)(void) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_FORK
	return ::fork();
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_FORK)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(vfork)(void) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_FORK
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	return ::vfork();
#pragma clang diagnostic pop
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_FORK)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC char *__SPRT_ID(ttyname)(int __fd) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return nullptr;
#else
	return ::ttyname(__fd);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(
		ttyname_r)(int __fd, char *__buf, __SPRT_ID(size_t) __buflen) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	__sprt_errno = ENOSYS;
	return -1;
#else
	return ::ttyname_r(__fd, __buf, __buflen);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(isatty)(int __fd) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return ::_isatty(__fd);
#else
	return ::isatty(__fd);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(link)(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return link(__from, __to);
}

__SPRT_C_FUNC int __SPRT_ID(symlink)(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return symlink(__from, __to);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(readlink)(const char *__SPRT_RESTRICT __path,
		char *__SPRT_RESTRICT __buf, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
	return readlink(__path, __buf, __len);
}
__SPRT_C_FUNC int __SPRT_ID(unlink)(const char *__name) __SPRT_NOEXCEPT { return unlink(__name); }

__SPRT_C_FUNC int __SPRT_ID(rmdir)(const char *__path) __SPRT_NOEXCEPT { return rmdir(__path); }

__SPRT_C_FUNC char *__SPRT_ID(getlogin)(void) { return getlogin(); }

__SPRT_C_FUNC int __SPRT_ID(getlogin_r)(char *__name, __SPRT_ID(size_t) __name_len) {
#if SPRT_ANDROID
	if (platform::_getlogin_r) {
		return platform::_getlogin_r(__name, __name_len);
	}
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return getlogin_r(__name, __name_len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(setlogin)(const char *__name) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_SETLOGIN
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETLOGIN)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::setlogin(__name);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(gethostname)(char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
	return ::gethostname(__name, __len);
}
__SPRT_C_FUNC int __SPRT_ID(
		sethostname)(const char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETHOSTNAME
	return ::sethostname(__name, __len);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETHOSTNAME)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(getdomainname)(char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return getdomainname(__name, __len);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(
		setdomainname)(const char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	errno = EPERM; // Operation not permitted
	return -1;
#else
	return ::setdomainname(__name, __len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fsync)(int __fd) { return fsync(__fd); }

__SPRT_C_FUNC void __SPRT_ID(sync)(void) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	_flushall();
#else
	return ::sync();
#endif
}

__SPRT_C_FUNC int __SPRT_ID(getpagesize)(void) __SPRT_NOEXCEPT { return getpagesize(); }
__SPRT_C_FUNC int __SPRT_ID(getdtablesize)(void) __SPRT_NOEXCEPT {
#if SPRT_ANDROID
	struct rlimit rlim;

	if (::getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
		return rlim.rlim_cur;
	} else {
		return -1;
	}
#else
	return getdtablesize();
#endif
}

__SPRT_C_FUNC int __SPRT_ID(truncate)(const char *__file, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT {
#if SPRT_MACOS
	return truncate(__file, length);
#else
	return truncate64(__file, length);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(ftruncate)(int __fd, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT {
#if SPRT_MACOS
	return ftruncate(__fd, length);
#else
	return ftruncate64(__fd, length);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(brk)(void *__addr) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_BRK
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#if SPRT_MACOS
	::brk(__addr);
	return 0;
#else
	return ::brk(__addr);
#endif
#pragma clang diagnostic pop
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_BRK)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC void *__SPRT_ID(sbrk)(__SPRT_ID(intptr_t) __delta) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_BRK
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	return ::sbrk(__delta);
#pragma clang diagnostic pop
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_BRK)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(lockf)(int __fd, int __cmd, __SPRT_ID(off_t) len) {
#if SPRT_MACOS
	return lockf(__fd, __cmd, len);
#else
	return lockf64(__fd, __cmd, len);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(copy_file_range)(int __infd, __SPRT_ID(off_t) * __pinoff, int __outfd,
				__SPRT_ID(off_t) * __poutoff, __SPRT_ID(size_t) __length, unsigned int __flags) {
#if __SPRT_CONFIG_HAVE_UNISTD_COPY_FILE_RANGE
#if SPRT_ANDROID
	if (platform::_copy_file_range) {
		return platform::_copy_file_range(__infd, __pinoff, __outfd, __poutoff, __length, __flags);
	}
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return copy_file_range(__infd, __pinoff, __outfd, __poutoff, __length, __flags);
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_COPY_FILE_RANGE)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(gettid)(void) {
#if SPRT_WINDOWS
	return GetCurrentThreadId();
#elif SPRT_MACOS
	return pthread_mach_thread_np(pthread_self());
#else
	return ::gettid();
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fdatasync)(int __fildes) {
#if SPRT_WINDOWS || SPRT_MACOS
	return fsync(__fildes);
#else
	return ::fdatasync(__fildes);
#endif
}

__SPRT_C_FUNC void __SPRT_ID(swab)(const void *__SPRT_RESTRICT __from, void *__SPRT_RESTRICT __to,
		__SPRT_ID(ssize_t) __n) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return _swab((char *)__from, (char *)__to, __n);
#else
	return ::swab(__from, __to, __n);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		symlinkat)(const char *__old_path, int __new_dir_fd, const char *__new_path) {
	return symlinkat(__old_path, __new_dir_fd, __new_path);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		readlinkat)(int __dir_fd, const char *__path, char *__buf, __SPRT_ID(size_t) __buf_size) {
	return readlinkat(__dir_fd, __path, __buf, __buf_size);
}

__SPRT_C_FUNC int __SPRT_ID(fchownat)(int __dir_fd, const char *__path, __SPRT_ID(uid_t) __owner,
		__SPRT_ID(gid_t) __group, int __flags) {
#if __SPRT_CONFIG_HAVE_UNISTD_CHOWN
	return ::fchownat(__dir_fd, __path, __owner, __group, __flags);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_CHOWN)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(faccessat)(int __dirfd, const char *__path, int __mode, int __flags) {
	return faccessat(__dirfd, __path, __mode, __flags);
}

__SPRT_C_FUNC int __SPRT_ID(linkat)(int __old_dir_fd, const char *__old_path, int __new_dir_fd,
		const char *__new_path, int __flags) {
	return linkat(__old_dir_fd, __old_path, __new_dir_fd, __new_path, __flags);
}

__SPRT_C_FUNC int __SPRT_ID(unlinkat)(int __dirfd, const char *__path, int __flags) {
	return unlinkat(__dirfd, __path, __flags);
}

__SPRT_C_FUNC long __SPRT_ID(gethostid)(void) {
#if SPRT_MACOS
	return gethostid();
#else
	auto dev = platform::getUniqueDeviceId();

	if constexpr (sizeof(long) == 4) {
		return hash32(dev.data(), dev.size());
	} else {
		return hash64(dev.data(), dev.size());
	}
#endif
}

__SPRT_C_FUNC int __SPRT_ID(pipe)(int fds[2]) { return pipe(fds); }

__SPRT_C_FUNC int __SPRT_ID(pipe2)(int fds[2], int flags) {
#if SPRT_MACOS
	static auto setnonblock = [](int fd) {
		int flags = fcntl(fd, F_GETFL, 0);
		if (flags == -1) {
			return -1;
		}
		flags |= O_NONBLOCK;
		if (fcntl(fd, F_SETFL, flags) == -1) {
			return -1;
		}
		return 0;
	};

	auto ret = pipe(fds);
	if (ret == 0) {
		if (flags & O_CLOEXEC) {
			fcntl(fds[0], F_SETFD, FD_CLOEXEC);
			fcntl(fds[1], F_SETFD, FD_CLOEXEC);
		}
		if (flags & O_NONBLOCK) {
			setnonblock(fds[0]);
			setnonblock(fds[1]);
		}
		return 0;
	}
	return ret;
#else
	return pipe2(fds, flags);
#endif
}

} // namespace sprt

#if __clang__
#pragma clang diagnostic pop
#endif
