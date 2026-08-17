// NuttX numbers _SC_*/_PC_* alphabetically from 1, not on the glibc table, so
// this cannot forward to linux_sprt. __sprt_sysconf()/__sprt_pathconf() pass the
// name argument straight through, and outside __SPRT_BUILD these macros ARE the
// _SC_*/_PC_* the application sees, so a Linux number here does not fail to be
// checked - it silently asks NuttX a different question (glibc _SC_PAGESIZE is
// 30, which is NuttX's _SC_COLL_WEIGHTS_MAX).
//
// Values from NuttX include/unistd.h. The names NuttX does not implement are
// marked "unsupported" and placed above its highest name (0x7e) rather than left
// at their Linux numbers, so that sysconf()/pathconf() answer EINVAL for them
// instead of silently returning an unrelated limit.

// clang-format off
#define __SPRT_SC_ARG_MAX 0x0014
#define __SPRT_SC_CHILD_MAX 0x001c
#define __SPRT_SC_CLK_TCK 0x001d
#define __SPRT_SC_NGROUPS_MAX 0x0034
#define __SPRT_SC_OPEN_MAX 0x0035
#define __SPRT_SC_STREAM_MAX 0x004a
#define __SPRT_SC_TZNAME_MAX 0x0068
#define __SPRT_SC_JOB_CONTROL 0x0029
#define __SPRT_SC_SAVED_IDS 0x003f
#define __SPRT_SC_REALTIME_SIGNALS 0x003c
#define __SPRT_SC_PRIORITY_SCHEDULING 0x0038
#define __SPRT_SC_TIMERS 0x005d
#define __SPRT_SC_ASYNCHRONOUS_IO 0x0015
#define __SPRT_SC_PRIORITIZED_IO 0x0037
#define __SPRT_SC_SYNCHRONIZED_IO 0x004c
#define __SPRT_SC_FSYNC 0x0023
#define __SPRT_SC_MAPPED_FILES 0x002c
#define __SPRT_SC_MEMLOCK 0x002d
#define __SPRT_SC_MEMLOCK_RANGE 0x002e
#define __SPRT_SC_MEMORY_PROTECTION 0x002f
#define __SPRT_SC_MESSAGE_PASSING 0x0030
#define __SPRT_SC_SEMAPHORES 0x0042
#define __SPRT_SC_SHARED_MEMORY_OBJECTS 0x0043
#define __SPRT_SC_AIO_LISTIO_MAX 0x0011
#define __SPRT_SC_AIO_MAX 0x0012
#define __SPRT_SC_AIO_PRIO_DELTA_MAX 0x0013
#define __SPRT_SC_DELAYTIMER_MAX 0x0021
#define __SPRT_SC_MQ_OPEN_MAX 0x0032
#define __SPRT_SC_MQ_PRIO_MAX 0x0033
#define __SPRT_SC_VERSION 0x006d
#define __SPRT_SC_PAGE_SIZE 0x0036
#define __SPRT_SC_PAGESIZE __SPRT_SC_PAGE_SIZE /* !! alias, as on NuttX */
#define __SPRT_SC_RTSIG_MAX 0x003e
#define __SPRT_SC_SEM_NSEMS_MAX 0x0040
#define __SPRT_SC_SEM_VALUE_MAX 0x0041
#define __SPRT_SC_SIGQUEUE_MAX 0x0045
#define __SPRT_SC_TIMER_MAX 0x005c
#define __SPRT_SC_BC_BASE_MAX 0x0018
#define __SPRT_SC_BC_DIM_MAX 0x0019
#define __SPRT_SC_BC_SCALE_MAX 0x001a
#define __SPRT_SC_BC_STRING_MAX 0x001b
#define __SPRT_SC_COLL_WEIGHTS_MAX 0x001f
#define __SPRT_SC_EXPR_NEST_MAX 0x0022
#define __SPRT_SC_LINE_MAX 0x002a
#define __SPRT_SC_RE_DUP_MAX 0x003a
#define __SPRT_SC_2_VERSION 0x000f
#define __SPRT_SC_2_C_BIND 0x0001
#define __SPRT_SC_2_C_DEV 0x0002
#define __SPRT_SC_2_FORT_DEV 0x0004
#define __SPRT_SC_2_FORT_RUN 0x0005
#define __SPRT_SC_2_SW_DEV 0x000d
#define __SPRT_SC_2_LOCALEDEF 0x0006
#define __SPRT_SC_UIO_MAXIOV 0x1000 // unsupported by NuttX
#define __SPRT_SC_IOV_MAX 0x0027
#define __SPRT_SC_THREADS 0x005a
#define __SPRT_SC_THREAD_SAFE_FUNCTIONS 0x0056
#define __SPRT_SC_GETGR_R_SIZE_MAX 0x0024
#define __SPRT_SC_GETPW_R_SIZE_MAX 0x0025
#define __SPRT_SC_LOGIN_NAME_MAX 0x002b
#define __SPRT_SC_TTY_NAME_MAX 0x0066
#define __SPRT_SC_THREAD_DESTRUCTOR_ITERATIONS 0x0050
#define __SPRT_SC_THREAD_KEYS_MAX 0x0051
#define __SPRT_SC_THREAD_STACK_MIN 0x0058
#define __SPRT_SC_THREAD_THREADS_MAX 0x0059
#define __SPRT_SC_THREAD_ATTR_STACKADDR 0x004d
#define __SPRT_SC_THREAD_ATTR_STACKSIZE 0x004e
#define __SPRT_SC_THREAD_PRIORITY_SCHEDULING 0x0054
#define __SPRT_SC_THREAD_PRIO_INHERIT 0x0052
#define __SPRT_SC_THREAD_PRIO_PROTECT 0x0053
#define __SPRT_SC_THREAD_PROCESS_SHARED 0x0055
#define __SPRT_SC_NPROCESSORS_CONF 0x007d
#define __SPRT_SC_NPROCESSORS_ONLN 0x007e
#define __SPRT_SC_PHYS_PAGES 0x007b
#define __SPRT_SC_AVPHYS_PAGES 0x007c
#define __SPRT_SC_ATEXIT_MAX 0x0016
#define __SPRT_SC_PASS_MAX 0x1001 // unsupported by NuttX
#define __SPRT_SC_XOPEN_VERSION 0x007a
#define __SPRT_SC_XOPEN_XCU_VERSION 0x1002 // unsupported by NuttX
#define __SPRT_SC_XOPEN_UNIX 0x0079
#define __SPRT_SC_XOPEN_CRYPT 0x0072
#define __SPRT_SC_XOPEN_ENH_I18N 0x0073
#define __SPRT_SC_XOPEN_SHM 0x0077
#define __SPRT_SC_2_CHAR_TERM 0x0003
#define __SPRT_SC_2_UPE 0x000e
#define __SPRT_SC_XOPEN_XPG2 0x1003 // unsupported by NuttX
#define __SPRT_SC_XOPEN_XPG3 0x1004 // unsupported by NuttX
#define __SPRT_SC_XOPEN_XPG4 0x1005 // unsupported by NuttX
#define __SPRT_SC_NZERO 0x1006 // unsupported by NuttX
#define __SPRT_SC_XBS5_ILP32_OFF32 0x006e
#define __SPRT_SC_XBS5_ILP32_OFFBIG 0x006f
#define __SPRT_SC_XBS5_LP64_OFF64 0x0070
#define __SPRT_SC_XBS5_LPBIG_OFFBIG 0x0071
#define __SPRT_SC_XOPEN_LEGACY 0x0074
#define __SPRT_SC_XOPEN_REALTIME 0x0075
#define __SPRT_SC_XOPEN_REALTIME_THREADS 0x0076
#define __SPRT_SC_ADVISORY_INFO 0x0010
#define __SPRT_SC_BARRIERS 0x0017
#define __SPRT_SC_CLOCK_SELECTION 0x001e
#define __SPRT_SC_CPUTIME 0x0020
#define __SPRT_SC_THREAD_CPUTIME 0x004f
#define __SPRT_SC_MONOTONIC_CLOCK 0x0031
#define __SPRT_SC_READER_WRITER_LOCKS 0x003b
#define __SPRT_SC_SPIN_LOCKS 0x0047
#define __SPRT_SC_REGEXP 0x003d
#define __SPRT_SC_SHELL 0x0044
#define __SPRT_SC_SPAWN 0x0046
#define __SPRT_SC_SPORADIC_SERVER 0x0048
#define __SPRT_SC_THREAD_SPORADIC_SERVER 0x0057
#define __SPRT_SC_TIMEOUTS 0x005b
#define __SPRT_SC_TYPED_MEMORY_OBJECTS 0x0067
#define __SPRT_SC_2_PBS 0x0007
#define __SPRT_SC_2_PBS_ACCOUNTING 0x0008
#define __SPRT_SC_2_PBS_LOCATE 0x000a
#define __SPRT_SC_2_PBS_MESSAGE 0x000b
#define __SPRT_SC_2_PBS_TRACK 0x000c
#define __SPRT_SC_SYMLOOP_MAX 0x004b
#define __SPRT_SC_STREAMS 0x1007 // unsupported by NuttX
#define __SPRT_SC_2_PBS_CHECKPOINT 0x0009
#define __SPRT_SC_V6_ILP32_OFF32 0x0069
#define __SPRT_SC_V6_ILP32_OFFBIG 0x006a
#define __SPRT_SC_V6_LP64_OFF64 0x006b
#define __SPRT_SC_V6_LPBIG_OFFBIG 0x006c
#define __SPRT_SC_HOST_NAME_MAX 0x0026
#define __SPRT_SC_TRACE 0x005e
#define __SPRT_SC_TRACE_EVENT_FILTER 0x005f
#define __SPRT_SC_TRACE_INHERIT 0x0061
#define __SPRT_SC_TRACE_LOG 0x0062
#define __SPRT_SC_IPV6 0x0028
#define __SPRT_SC_RAW_SOCKETS 0x0039
#define __SPRT_SC_V7_ILP32_OFF32 0x1008 // unsupported by NuttX
#define __SPRT_SC_V7_ILP32_OFFBIG 0x1009 // unsupported by NuttX
#define __SPRT_SC_V7_LP64_OFF64 0x100a // unsupported by NuttX
#define __SPRT_SC_V7_LPBIG_OFFBIG 0x100b // unsupported by NuttX
#define __SPRT_SC_SS_REPL_MAX 0x0049
#define __SPRT_SC_TRACE_EVENT_NAME_MAX 0x0060
#define __SPRT_SC_TRACE_NAME_MAX 0x0063
#define __SPRT_SC_TRACE_SYS_MAX 0x0064
#define __SPRT_SC_TRACE_USER_EVENT_MAX 0x0065
#define __SPRT_SC_XOPEN_STREAMS 0x0078
#define __SPRT_SC_THREAD_ROBUST_PRIO_INHERIT 0x100c // unsupported by NuttX
#define __SPRT_SC_THREAD_ROBUST_PRIO_PROTECT 0x100d // unsupported by NuttX
#define __SPRT_SC_MINSIGSTKSZ 0x100e // unsupported by NuttX
#define __SPRT_SC_SIGSTKSZ 0x100f // unsupported by NuttX

#define __SPRT_PC_LINK_MAX 0x0006
#define __SPRT_PC_MAX_CANON 0x0007
#define __SPRT_PC_MAX_INPUT 0x0008
#define __SPRT_PC_NAME_MAX 0x0009
#define __SPRT_PC_PATH_MAX 0x000b
#define __SPRT_PC_PIPE_BUF 0x000c
#define __SPRT_PC_CHOWN_RESTRICTED 0x0004
#define __SPRT_PC_NO_TRUNC 0x000a
#define __SPRT_PC_VDISABLE 0x0013
#define __SPRT_PC_SYNC_IO 0x0012
#define __SPRT_PC_ASYNC_IO 0x0003
#define __SPRT_PC_PRIO_IO 0x000d
#define __SPRT_PC_SOCK_MAXBUF 0x1010 // unsupported by NuttX
#define __SPRT_PC_FILESIZEBITS 0x0005
#define __SPRT_PC_REC_INCR_XFER_SIZE 0x000e
#define __SPRT_PC_REC_MAX_XFER_SIZE 0x1011 // unsupported by NuttX
#define __SPRT_PC_REC_MIN_XFER_SIZE 0x000f
#define __SPRT_PC_REC_XFER_ALIGN 0x0010
#define __SPRT_PC_ALLOC_SIZE_MIN 0x0002
#define __SPRT_PC_SYMLINK_MAX 0x0011
#define __SPRT_PC_2_SYMLINKS 0x0001

// clang-format on
