// Embox declares only a handful of _SC_* names, with its own numbering
// (src/compat/posix/include/unistd.h), and no _PC_* at all - so this cannot
// forward to linux_sprt. __sprt_sysconf()/__sprt_pathconf() pass the name
// argument straight through, and outside __SPRT_BUILD these macros ARE the
// _SC_*/_PC_* the application sees, so a Linux number here would not merely go
// unchecked - it would silently ask Embox a different question (glibc
// _SC_PAGESIZE is 30, Embox's is 1).
//
// The names Embox does implement carry its values; everything else is marked
// "unsupported" and numbered from 0x1000 up - far above every name Embox knows -
// so sysconf()/pathconf() answer -1 for them instead of returning an unrelated
// limit. The wrapper's asserts are #ifdef'd on the native spelling, so only the
// implemented names below are pinned.

// clang-format off
#define __SPRT_SC_ARG_MAX 0x1000 // unsupported
#define __SPRT_SC_CHILD_MAX 0x1001 // unsupported
#define __SPRT_SC_CLK_TCK 2
#define __SPRT_SC_NGROUPS_MAX 0x1002 // unsupported
#define __SPRT_SC_OPEN_MAX 20
#define __SPRT_SC_STREAM_MAX 0x1003 // unsupported
#define __SPRT_SC_TZNAME_MAX 0x1004 // unsupported
#define __SPRT_SC_JOB_CONTROL 0x1005 // unsupported
#define __SPRT_SC_SAVED_IDS 0x1006 // unsupported
#define __SPRT_SC_REALTIME_SIGNALS 0x1007 // unsupported
#define __SPRT_SC_PRIORITY_SCHEDULING 0x1008 // unsupported
#define __SPRT_SC_TIMERS 0x1009 // unsupported
#define __SPRT_SC_ASYNCHRONOUS_IO 0x100a // unsupported
#define __SPRT_SC_PRIORITIZED_IO 0x100b // unsupported
#define __SPRT_SC_SYNCHRONIZED_IO 0x100c // unsupported
#define __SPRT_SC_FSYNC 0x100d // unsupported
#define __SPRT_SC_MAPPED_FILES 5
#define __SPRT_SC_MEMLOCK 0x100e // unsupported
#define __SPRT_SC_MEMLOCK_RANGE 0x100f // unsupported
#define __SPRT_SC_MEMORY_PROTECTION 0x1010 // unsupported
#define __SPRT_SC_MESSAGE_PASSING 0x1011 // unsupported
#define __SPRT_SC_SEMAPHORES 0x1012 // unsupported
#define __SPRT_SC_SHARED_MEMORY_OBJECTS 0x1013 // unsupported
#define __SPRT_SC_AIO_LISTIO_MAX 0x1014 // unsupported
#define __SPRT_SC_AIO_MAX 0x1015 // unsupported
#define __SPRT_SC_AIO_PRIO_DELTA_MAX 0x1016 // unsupported
#define __SPRT_SC_DELAYTIMER_MAX 0x1017 // unsupported
#define __SPRT_SC_MQ_OPEN_MAX 0x1018 // unsupported
#define __SPRT_SC_MQ_PRIO_MAX 0x1019 // unsupported
#define __SPRT_SC_VERSION 0x101a // unsupported
#define __SPRT_SC_PAGE_SIZE __SPRT_SC_PAGESIZE
#define __SPRT_SC_PAGESIZE 1
#define __SPRT_SC_RTSIG_MAX 0x101b // unsupported
#define __SPRT_SC_SEM_NSEMS_MAX 0x101c // unsupported
#define __SPRT_SC_SEM_VALUE_MAX 0x101d // unsupported
#define __SPRT_SC_SIGQUEUE_MAX 0x101e // unsupported
#define __SPRT_SC_TIMER_MAX 0x101f // unsupported
#define __SPRT_SC_BC_BASE_MAX 0x1020 // unsupported
#define __SPRT_SC_BC_DIM_MAX 0x1021 // unsupported
#define __SPRT_SC_BC_SCALE_MAX 0x1022 // unsupported
#define __SPRT_SC_BC_STRING_MAX 0x1023 // unsupported
#define __SPRT_SC_COLL_WEIGHTS_MAX 0x1024 // unsupported
#define __SPRT_SC_EXPR_NEST_MAX 0x1025 // unsupported
#define __SPRT_SC_LINE_MAX 0x1026 // unsupported
#define __SPRT_SC_RE_DUP_MAX 0x1027 // unsupported
#define __SPRT_SC_2_VERSION 0x1028 // unsupported
#define __SPRT_SC_2_C_BIND 0x1029 // unsupported
#define __SPRT_SC_2_C_DEV 0x102a // unsupported
#define __SPRT_SC_2_FORT_DEV 0x102b // unsupported
#define __SPRT_SC_2_FORT_RUN 0x102c // unsupported
#define __SPRT_SC_2_SW_DEV 0x102d // unsupported
#define __SPRT_SC_2_LOCALEDEF 0x102e // unsupported
#define __SPRT_SC_UIO_MAXIOV 0x102f // unsupported
#define __SPRT_SC_IOV_MAX 0x1030 // unsupported
#define __SPRT_SC_THREADS 0x1031 // unsupported
#define __SPRT_SC_THREAD_SAFE_FUNCTIONS 0x1032 // unsupported
#define __SPRT_SC_GETGR_R_SIZE_MAX 0x1033 // unsupported
#define __SPRT_SC_GETPW_R_SIZE_MAX 3
#define __SPRT_SC_LOGIN_NAME_MAX 0x1034 // unsupported
#define __SPRT_SC_TTY_NAME_MAX 0x1035 // unsupported
#define __SPRT_SC_THREAD_DESTRUCTOR_ITERATIONS 0x1036 // unsupported
#define __SPRT_SC_THREAD_KEYS_MAX 0x1037 // unsupported
#define __SPRT_SC_THREAD_STACK_MIN 0x1038 // unsupported
#define __SPRT_SC_THREAD_THREADS_MAX 0x1039 // unsupported
#define __SPRT_SC_THREAD_ATTR_STACKADDR 0x103a // unsupported
#define __SPRT_SC_THREAD_ATTR_STACKSIZE 0x103b // unsupported
#define __SPRT_SC_THREAD_PRIORITY_SCHEDULING 0x103c // unsupported
#define __SPRT_SC_THREAD_PRIO_INHERIT 0x103d // unsupported
#define __SPRT_SC_THREAD_PRIO_PROTECT 0x103e // unsupported
#define __SPRT_SC_THREAD_PROCESS_SHARED 0x103f // unsupported
#define __SPRT_SC_NPROCESSORS_CONF __SPRT_SC_NPROCESSORS_ONLN
#define __SPRT_SC_NPROCESSORS_ONLN 103
#define __SPRT_SC_PHYS_PAGES 104
#define __SPRT_SC_AVPHYS_PAGES 0x1040 // unsupported
#define __SPRT_SC_ATEXIT_MAX 4
#define __SPRT_SC_PASS_MAX 0x1041 // unsupported
#define __SPRT_SC_XOPEN_VERSION 0x1042 // unsupported
#define __SPRT_SC_XOPEN_XCU_VERSION 0x1043 // unsupported
#define __SPRT_SC_XOPEN_UNIX 0x1044 // unsupported
#define __SPRT_SC_XOPEN_CRYPT 0x1045 // unsupported
#define __SPRT_SC_XOPEN_ENH_I18N 0x1046 // unsupported
#define __SPRT_SC_XOPEN_SHM 0x1047 // unsupported
#define __SPRT_SC_2_CHAR_TERM 0x1048 // unsupported
#define __SPRT_SC_2_UPE 0x1049 // unsupported
#define __SPRT_SC_XOPEN_XPG2 0x104a // unsupported
#define __SPRT_SC_XOPEN_XPG3 0x104b // unsupported
#define __SPRT_SC_XOPEN_XPG4 0x104c // unsupported
#define __SPRT_SC_NZERO 0x104d // unsupported
#define __SPRT_SC_XBS5_ILP32_OFF32 0x104e // unsupported
#define __SPRT_SC_XBS5_ILP32_OFFBIG 0x104f // unsupported
#define __SPRT_SC_XBS5_LP64_OFF64 0x1050 // unsupported
#define __SPRT_SC_XBS5_LPBIG_OFFBIG 0x1051 // unsupported
#define __SPRT_SC_XOPEN_LEGACY 0x1052 // unsupported
#define __SPRT_SC_XOPEN_REALTIME 0x1053 // unsupported
#define __SPRT_SC_XOPEN_REALTIME_THREADS 0x1054 // unsupported
#define __SPRT_SC_ADVISORY_INFO 0x1055 // unsupported
#define __SPRT_SC_BARRIERS 0x1056 // unsupported
#define __SPRT_SC_CLOCK_SELECTION 0x1057 // unsupported
#define __SPRT_SC_CPUTIME 0x1058 // unsupported
#define __SPRT_SC_THREAD_CPUTIME 0x1059 // unsupported
#define __SPRT_SC_MONOTONIC_CLOCK 0x105a // unsupported
#define __SPRT_SC_READER_WRITER_LOCKS 0x105b // unsupported
#define __SPRT_SC_SPIN_LOCKS 0x105c // unsupported
#define __SPRT_SC_REGEXP 0x105d // unsupported
#define __SPRT_SC_SHELL 0x105e // unsupported
#define __SPRT_SC_SPAWN 0x105f // unsupported
#define __SPRT_SC_SPORADIC_SERVER 0x1060 // unsupported
#define __SPRT_SC_THREAD_SPORADIC_SERVER 0x1061 // unsupported
#define __SPRT_SC_TIMEOUTS 0x1062 // unsupported
#define __SPRT_SC_TYPED_MEMORY_OBJECTS 0x1063 // unsupported
#define __SPRT_SC_2_PBS 0x1064 // unsupported
#define __SPRT_SC_2_PBS_ACCOUNTING 0x1065 // unsupported
#define __SPRT_SC_2_PBS_LOCATE 0x1066 // unsupported
#define __SPRT_SC_2_PBS_MESSAGE 0x1067 // unsupported
#define __SPRT_SC_2_PBS_TRACK 0x1068 // unsupported
#define __SPRT_SC_SYMLOOP_MAX 0x1069 // unsupported
#define __SPRT_SC_STREAMS 0x106a // unsupported
#define __SPRT_SC_2_PBS_CHECKPOINT 0x106b // unsupported
#define __SPRT_SC_V6_ILP32_OFF32 0x106c // unsupported
#define __SPRT_SC_V6_ILP32_OFFBIG 0x106d // unsupported
#define __SPRT_SC_V6_LP64_OFF64 0x106e // unsupported
#define __SPRT_SC_V6_LPBIG_OFFBIG 0x106f // unsupported
#define __SPRT_SC_HOST_NAME_MAX 0x1070 // unsupported
#define __SPRT_SC_TRACE 0x1071 // unsupported
#define __SPRT_SC_TRACE_EVENT_FILTER 0x1072 // unsupported
#define __SPRT_SC_TRACE_INHERIT 0x1073 // unsupported
#define __SPRT_SC_TRACE_LOG 0x1074 // unsupported
#define __SPRT_SC_IPV6 0x1075 // unsupported
#define __SPRT_SC_RAW_SOCKETS 0x1076 // unsupported
#define __SPRT_SC_V7_ILP32_OFF32 0x1077 // unsupported
#define __SPRT_SC_V7_ILP32_OFFBIG 0x1078 // unsupported
#define __SPRT_SC_V7_LP64_OFF64 0x1079 // unsupported
#define __SPRT_SC_V7_LPBIG_OFFBIG 0x107a // unsupported
#define __SPRT_SC_SS_REPL_MAX 0x107b // unsupported
#define __SPRT_SC_TRACE_EVENT_NAME_MAX 0x107c // unsupported
#define __SPRT_SC_TRACE_NAME_MAX 0x107d // unsupported
#define __SPRT_SC_TRACE_SYS_MAX 0x107e // unsupported
#define __SPRT_SC_TRACE_USER_EVENT_MAX 0x107f // unsupported
#define __SPRT_SC_XOPEN_STREAMS 0x1080 // unsupported
#define __SPRT_SC_THREAD_ROBUST_PRIO_INHERIT 0x1081 // unsupported
#define __SPRT_SC_THREAD_ROBUST_PRIO_PROTECT 0x1082 // unsupported
#define __SPRT_SC_MINSIGSTKSZ 0x1083 // unsupported
#define __SPRT_SC_SIGSTKSZ 0x1084 // unsupported

#define __SPRT_PC_LINK_MAX 0x1085 // unsupported
#define __SPRT_PC_MAX_CANON 0x1086 // unsupported
#define __SPRT_PC_MAX_INPUT 0x1087 // unsupported
#define __SPRT_PC_NAME_MAX 0x1088 // unsupported
#define __SPRT_PC_PATH_MAX 0x1089 // unsupported
#define __SPRT_PC_PIPE_BUF 0x108a // unsupported
#define __SPRT_PC_CHOWN_RESTRICTED 0x108b // unsupported
#define __SPRT_PC_NO_TRUNC 0x108c // unsupported
#define __SPRT_PC_VDISABLE 0x108d // unsupported
#define __SPRT_PC_SYNC_IO 0x108e // unsupported
#define __SPRT_PC_ASYNC_IO 0x108f // unsupported
#define __SPRT_PC_PRIO_IO 0x1090 // unsupported
#define __SPRT_PC_SOCK_MAXBUF 0x1091 // unsupported
#define __SPRT_PC_FILESIZEBITS 0x1092 // unsupported
#define __SPRT_PC_REC_INCR_XFER_SIZE 0x1093 // unsupported
#define __SPRT_PC_REC_MAX_XFER_SIZE 0x1094 // unsupported
#define __SPRT_PC_REC_MIN_XFER_SIZE 0x1095 // unsupported
#define __SPRT_PC_REC_XFER_ALIGN 0x1096 // unsupported
#define __SPRT_PC_ALLOC_SIZE_MIN 0x1097 // unsupported
#define __SPRT_PC_SYMLINK_MAX 0x1098 // unsupported
#define __SPRT_PC_2_SYMLINKS 0x1099 // unsupported
// clang-format on
