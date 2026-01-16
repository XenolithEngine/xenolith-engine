/** sysconf() query for the maximum number of bytes of exec() arguments. */
#define __SPRT_SC_ARG_MAX 0x0000
/** sysconf() query for bc(1) behavior equivalent to _POSIX2_BC_BASE_MAX. */
#define __SPRT_SC_BC_BASE_MAX 0x0001
/** sysconf() query for bc(1) behavior equivalent to _POSIX2_BC_DIM_MAX. */
#define __SPRT_SC_BC_DIM_MAX 0x0002
/** sysconf() query for bc(1) behavior equivalent to _POSIX2_BC_SCALE_MAX. */
#define __SPRT_SC_BC_SCALE_MAX 0x0003
/** sysconf() query for bc(1) behavior equivalent to _POSIX2_BC_STRING_MAX. */
#define __SPRT_SC_BC_STRING_MAX 0x0004
/** sysconf() query equivalent to RLIMIT_NPROC. */
#define __SPRT_SC_CHILD_MAX 0x0005
/** sysconf() query equivalent to AT_CLKTCK. */
#define __SPRT_SC_CLK_TCK 0x0006
/** sysconf() query for collation behavior equivalent to _POSIX2_COLL_WEIGHTS_MAX. */
#define __SPRT_SC_COLL_WEIGHTS_MAX 0x0007
/** sysconf() query for expr(1) behavior equivalent to _POSIX2_EXPR_NEST_MAX. */
#define __SPRT_SC_EXPR_NEST_MAX 0x0008
/** sysconf() query for command-line tool behavior equivalent to _POSIX2_LINE_MAX. */
#define __SPRT_SC_LINE_MAX 0x0009
/** sysconf() query equivalent to NGROUPS_MAX. */
#define __SPRT_SC_NGROUPS_MAX 0x000a
/** sysconf() query equivalent to RLIMIT_NOFILE. */
#define __SPRT_SC_OPEN_MAX 0x000b
/** sysconf() query equivalent to PASS_MAX. */
#define __SPRT_SC_PASS_MAX 0x000c
/** sysconf() query equivalent to _POSIX2_C_BIND. */
#define __SPRT_SC_2_C_BIND 0x000d
/** sysconf() query equivalent to _POSIX2_C_DEV. */
#define __SPRT_SC_2_C_DEV 0x000e
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_2_C_VERSION 0x000f
/** sysconf() query equivalent to _POSIX2_CHAR_TERM. */
#define __SPRT_SC_2_CHAR_TERM 0x0010
/** sysconf() query equivalent to _POSIX2_FORT_DEV. */
#define __SPRT_SC_2_FORT_DEV 0x0011
/** sysconf() query equivalent to _POSIX2_FORT_RUN. */
#define __SPRT_SC_2_FORT_RUN 0x0012
/** sysconf() query equivalent to _POSIX2_LOCALEDEF. */
#define __SPRT_SC_2_LOCALEDEF 0x0013
/** sysconf() query equivalent to _POSIX2_SW_DEV. */
#define __SPRT_SC_2_SW_DEV 0x0014
/** sysconf() query equivalent to _POSIX2_UPE. */
#define __SPRT_SC_2_UPE 0x0015
/** sysconf() query equivalent to _POSIX2_VERSION. */
#define __SPRT_SC_2_VERSION 0x0016
/** sysconf() query equivalent to _POSIX_JOB_CONTROL. */
#define __SPRT_SC_JOB_CONTROL 0x0017
/** sysconf() query equivalent to _POSIX_SAVED_IDS. */
#define __SPRT_SC_SAVED_IDS 0x0018
/** sysconf() query equivalent to _POSIX_VERSION. */
#define __SPRT_SC_VERSION 0x0019
/** sysconf() query equivalent to _POSIX_RE_DUP_MAX. */
#define __SPRT_SC_RE_DUP_MAX 0x001a
/** sysconf() query equivalent to FOPEN_MAX. */
#define __SPRT_SC_STREAM_MAX 0x001b
/** sysconf() query equivalent to _POSIX_TZNAME_MAX. */
#define __SPRT_SC_TZNAME_MAX 0x001c
/** sysconf() query equivalent to _XOPEN_CRYPT. */
#define __SPRT_SC_XOPEN_CRYPT 0x001d
/** sysconf() query equivalent to _XOPEN_ENH_I18N. */
#define __SPRT_SC_XOPEN_ENH_I18N 0x001e
/** sysconf() query equivalent to _XOPEN_SHM. */
#define __SPRT_SC_XOPEN_SHM 0x001f
/** sysconf() query equivalent to _XOPEN_VERSION. */
#define __SPRT_SC_XOPEN_VERSION 0x0020
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_XOPEN_XCU_VERSION 0x0021
/** sysconf() query equivalent to _XOPEN_REALTIME. */
#define __SPRT_SC_XOPEN_REALTIME 0x0022
/** sysconf() query equivalent to _XOPEN_REALTIME_THREADS. */
#define __SPRT_SC_XOPEN_REALTIME_THREADS 0x0023
/** sysconf() query equivalent to _XOPEN_LEGACY. */
#define __SPRT_SC_XOPEN_LEGACY 0x0024
/** sysconf() query for the maximum number of atexit() handlers. Unlimited on Android. */
#define __SPRT_SC_ATEXIT_MAX 0x0025
/** sysconf() query equivalent to IOV_MAX. */
#define __SPRT_SC_IOV_MAX 0x0026
/** Same as __SPRT_SC_IOV_MAX. */
#define __SPRT_SC_UIO_MAXIOV __SPRT_SC_IOV_MAX
/** Same as __SPRT_SC_PAGE_SIZE. */
#define __SPRT_SC_PAGESIZE 0x0027
/** sysconf() query equivalent to getpagesize(). */
#define __SPRT_SC_PAGE_SIZE 0x0028
/** sysconf() query equivalent to _XOPEN_UNIX. */
#define __SPRT_SC_XOPEN_UNIX 0x0029
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_XBS5_ILP32_OFF32 0x002a
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_XBS5_ILP32_OFFBIG 0x002b
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_XBS5_LP64_OFF64 0x002c
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_XBS5_LPBIG_OFFBIG 0x002d
/** sysconf() query equivalent to _POSIX_AIO_LISTIO_MAX. */
#define __SPRT_SC_AIO_LISTIO_MAX 0x002e
/** sysconf() query equivalent to _POSIX_AIO_MAX. */
#define __SPRT_SC_AIO_MAX 0x002f
/** Unimplemented on Android. */
#define __SPRT_SC_AIO_PRIO_DELTA_MAX  0x0030
/** sysconf() query equivalent to _POSIX_DELAYTIMER_MAX. */
#define __SPRT_SC_DELAYTIMER_MAX 0x0031
/** sysconf() query equivalent to _POSIX_MQ_OPEN_MAX. */
#define __SPRT_SC_MQ_OPEN_MAX 0x0032
/** sysconf() query equivalent to _POSIX_MQ_PRIO_MAX. */
#define __SPRT_SC_MQ_PRIO_MAX 0x0033
/** sysconf() query equivalent to RTSIG_MAX. Constant on Android. */
#define __SPRT_SC_RTSIG_MAX 0x0034
/** sysconf() query equivalent to _POSIX_SEM_NSEMS_MAX. Constant on Android. */
#define __SPRT_SC_SEM_NSEMS_MAX 0x0035
/** sysconf() query equivalent to SEM_VALUE_MAX. Constant on Android. */
#define __SPRT_SC_SEM_VALUE_MAX 0x0036
/** sysconf() query equivalent to _POSIX_SIGQUEUE_MAX. */
#define __SPRT_SC_SIGQUEUE_MAX 0x0037
/** sysconf() query equivalent to _POSIX_TIMER_MAX. */
#define __SPRT_SC_TIMER_MAX 0x0038
/** sysconf() query equivalent to _POSIX_ASYNCHRONOUS_IO. */
#define __SPRT_SC_ASYNCHRONOUS_IO 0x0039
/** sysconf() query equivalent to _POSIX_FSYNC. */
#define __SPRT_SC_FSYNC 0x003a
/** sysconf() query equivalent to _POSIX_MAPPED_FILES. */
#define __SPRT_SC_MAPPED_FILES 0x003b
/** sysconf() query equivalent to _POSIX_MEMLOCK. */
#define __SPRT_SC_MEMLOCK 0x003c
/** sysconf() query equivalent to _POSIX_MEMLOCK_RANGE. */
#define __SPRT_SC_MEMLOCK_RANGE 0x003d
/** sysconf() query equivalent to _POSIX_MEMORY_PROTECTION. */
#define __SPRT_SC_MEMORY_PROTECTION 0x003e
/** sysconf() query equivalent to _POSIX_MESSAGE_PASSING. */
#define __SPRT_SC_MESSAGE_PASSING 0x003f
/** sysconf() query equivalent to _POSIX_PRIORITIZED_IO. */
#define __SPRT_SC_PRIORITIZED_IO 0x0040
/** sysconf() query equivalent to _POSIX_PRIORITY_SCHEDULING. */
#define __SPRT_SC_PRIORITY_SCHEDULING 0x0041
/** sysconf() query equivalent to _POSIX_REALTIME_SIGNALS. */
#define __SPRT_SC_REALTIME_SIGNALS 0x0042
/** sysconf() query equivalent to _POSIX_SEMAPHORES. */
#define __SPRT_SC_SEMAPHORES 0x0043
/** sysconf() query equivalent to _POSIX_SHARED_MEMORY_OBJECTS. */
#define __SPRT_SC_SHARED_MEMORY_OBJECTS 0x0044
/** sysconf() query equivalent to _POSIX_SYNCHRONIZED_IO. */
#define __SPRT_SC_SYNCHRONIZED_IO 0x0045
/** sysconf() query equivalent to _POSIX_TIMERS. */
#define __SPRT_SC_TIMERS 0x0046
/** sysconf() query for an initial size for getgrgid_r() and getgrnam_r() buffers. */
#define __SPRT_SC_GETGR_R_SIZE_MAX 0x0047
/** sysconf() query for an initial size for getpwuid_r() and getpwnam_r() buffers. */
#define __SPRT_SC_GETPW_R_SIZE_MAX 0x0048
/** sysconf() query equivalent to LOGIN_NAME_MAX. */
#define __SPRT_SC_LOGIN_NAME_MAX 0x0049
/** sysconf() query equivalent to PTHREAD_DESTRUCTOR_ITERATIONS. */
#define __SPRT_SC_THREAD_DESTRUCTOR_ITERATIONS 0x004a
/** sysconf() query equivalent to PTHREAD_KEYS_MAX. */
#define __SPRT_SC_THREAD_KEYS_MAX 0x004b
/** sysconf() query equivalent to PTHREAD_STACK_MIN. */
#define __SPRT_SC_THREAD_STACK_MIN 0x004c
/** sysconf() query for a maximum number of threads. Unlimited on Android. */
#define __SPRT_SC_THREAD_THREADS_MAX 0x004d
/** sysconf() query equivalent to TTY_NAME_MAX. */
#define __SPRT_SC_TTY_NAME_MAX 0x004e
/** sysconf() query equivalent to _POSIX_THREADS. */
#define __SPRT_SC_THREADS 0x004f
/** sysconf() query equivalent to _POSIX_THREAD_ATTR_STACKADDR. */
#define __SPRT_SC_THREAD_ATTR_STACKADDR 0x0050
/** sysconf() query equivalent to _POSIX_THREAD_ATTR_STACKSIZE. */
#define __SPRT_SC_THREAD_ATTR_STACKSIZE 0x0051
/** sysconf() query equivalent to _POSIX_THREAD_PRIORITY_SCHEDULING. */
#define __SPRT_SC_THREAD_PRIORITY_SCHEDULING 0x0052
/** sysconf() query equivalent to _POSIX_THREAD_PRIO_INHERIT. */
#define __SPRT_SC_THREAD_PRIO_INHERIT 0x0053
/** sysconf() query equivalent to _POSIX_THREAD_PRIO_PROTECT. */
#define __SPRT_SC_THREAD_PRIO_PROTECT 0x0054
/** sysconf() query equivalent to _POSIX_THREAD_SAFE_FUNCTIONS. */
#define __SPRT_SC_THREAD_SAFE_FUNCTIONS 0x0055
/** sysconf() query equivalent to get_nprocs_conf(). */
#define __SPRT_SC_NPROCESSORS_CONF 0x0060
/** sysconf() query equivalent to get_nprocs(). */
#define __SPRT_SC_NPROCESSORS_ONLN 0x0061
/** sysconf() query equivalent to get_phys_pages(). */
#define __SPRT_SC_PHYS_PAGES 0x0062
/** sysconf() query equivalent to get_avphys_pages(). */
#define __SPRT_SC_AVPHYS_PAGES 0x0063
/** sysconf() query equivalent to _POSIX_MONOTONIC_CLOCK. */
#define __SPRT_SC_MONOTONIC_CLOCK 0x0064
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_2_PBS 0x0065
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_2_PBS_ACCOUNTING 0x0066
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_2_PBS_CHECKPOINT 0x0067
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_2_PBS_LOCATE 0x0068
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_2_PBS_MESSAGE 0x0069
/** Obsolescent in POSIX.1-2008. */
#define __SPRT_SC_2_PBS_TRACK 0x006a
/** sysconf() query equivalent to _POSIX_ADVISORY_INFO. */
#define __SPRT_SC_ADVISORY_INFO 0x006b
/** sysconf() query equivalent to _POSIX_BARRIERS. */
#define __SPRT_SC_BARRIERS 0x006c
/** sysconf() query equivalent to _POSIX_CLOCK_SELECTION. */
#define __SPRT_SC_CLOCK_SELECTION 0x006d
/** sysconf() query equivalent to _POSIX_CPUTIME. */
#define __SPRT_SC_CPUTIME 0x006e
/** sysconf() query equivalent to _POSIX_HOST_NAME_MAX. */
#define __SPRT_SC_HOST_NAME_MAX 0x006f
/** sysconf() query equivalent to _POSIX_IPV6. */
#define __SPRT_SC_IPV6 0x0070
/** sysconf() query equivalent to _POSIX_RAW_SOCKETS. */
#define __SPRT_SC_RAW_SOCKETS 0x0071
/** sysconf() query equivalent to _POSIX_READER_WRITER_LOCKS. */
#define __SPRT_SC_READER_WRITER_LOCKS 0x0072
/** sysconf() query equivalent to _POSIX_REGEXP. */
#define __SPRT_SC_REGEXP 0x0073
/** sysconf() query equivalent to _POSIX_SHELL. */
#define __SPRT_SC_SHELL 0x0074
/** sysconf() query equivalent to _POSIX_SPAWN. */
#define __SPRT_SC_SPAWN 0x0075
/** sysconf() query equivalent to _POSIX_SPIN_LOCKS. */
#define __SPRT_SC_SPIN_LOCKS 0x0076
/** sysconf() query equivalent to _POSIX_SPORADIC_SERVER. */
#define __SPRT_SC_SPORADIC_SERVER 0x0077
/** sysconf() query equivalent to _POSIX_SS_REPL_MAX. */
#define __SPRT_SC_SS_REPL_MAX 0x0078
/** sysconf() query equivalent to _POSIX_SYMLOOP_MAX. */
#define __SPRT_SC_SYMLOOP_MAX 0x0079
/** sysconf() query equivalent to _POSIX_THREAD_CPUTIME. */
#define __SPRT_SC_THREAD_CPUTIME 0x007a
/** sysconf() query equivalent to _POSIX_THREAD_PROCESS_SHARED. */
#define __SPRT_SC_THREAD_PROCESS_SHARED 0x007b
/** sysconf() query equivalent to _POSIX_THREAD_ROBUST_PRIO_INHERIT. */
#define __SPRT_SC_THREAD_ROBUST_PRIO_INHERIT 0x007c
/** sysconf() query equivalent to _POSIX_THREAD_ROBUST_PRIO_PROTECT. */
#define __SPRT_SC_THREAD_ROBUST_PRIO_PROTECT 0x007d
/** sysconf() query equivalent to _POSIX_THREAD_SPORADIC_SERVER. */
#define __SPRT_SC_THREAD_SPORADIC_SERVER 0x007e
/** sysconf() query equivalent to _POSIX_TIMEOUTS. */
#define __SPRT_SC_TIMEOUTS 0x007f
