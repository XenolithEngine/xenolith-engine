#define __SPRT_SC_ARG_MAX			 1
#define __SPRT_SC_CHILD_MAX			 2
#define __SPRT_SC_CLK_TCK			 3
#define __SPRT_SC_NGROUPS_MAX			 4
#define __SPRT_SC_OPEN_MAX			 5
#define __SPRT_SC_JOB_CONTROL			 6
#define __SPRT_SC_SAVED_IDS			 7
#define __SPRT_SC_VERSION			 8
#define __SPRT_SC_BC_BASE_MAX			 9
#define __SPRT_SC_BC_DIM_MAX			10
#define __SPRT_SC_BC_SCALE_MAX		11
#define __SPRT_SC_BC_STRING_MAX		12
#define __SPRT_SC_COLL_WEIGHTS_MAX		13
#define __SPRT_SC_EXPR_NEST_MAX		14
#define __SPRT_SC_LINE_MAX			15
#define __SPRT_SC_RE_DUP_MAX			16
#define __SPRT_SC_2_VERSION			17
#define __SPRT_SC_2_C_BIND			18
#define __SPRT_SC_2_C_DEV			19
#define __SPRT_SC_2_CHAR_TERM			20
#define __SPRT_SC_2_FORT_DEV			21
#define __SPRT_SC_2_FORT_RUN			22
#define __SPRT_SC_2_LOCALEDEF			23
#define __SPRT_SC_2_SW_DEV			24
#define __SPRT_SC_2_UPE			25
#define __SPRT_SC_STREAM_MAX			26
#define __SPRT_SC_TZNAME_MAX			27
#define __SPRT_SC_ASYNCHRONOUS_IO		28
#define __SPRT_SC_PAGESIZE			29
#define __SPRT_SC_MEMLOCK			30
#define __SPRT_SC_MEMLOCK_RANGE		31
#define __SPRT_SC_MEMORY_PROTECTION		32
#define __SPRT_SC_MESSAGE_PASSING		33
#define __SPRT_SC_PRIORITIZED_IO		34
#define __SPRT_SC_PRIORITY_SCHEDULING		35
#define __SPRT_SC_REALTIME_SIGNALS		36
#define __SPRT_SC_SEMAPHORES			37
#define __SPRT_SC_FSYNC			38
#define __SPRT_SC_SHARED_MEMORY_OBJECTS 	39
#define __SPRT_SC_SYNCHRONIZED_IO		40
#define __SPRT_SC_TIMERS			41
#define __SPRT_SC_AIO_LISTIO_MAX		42
#define __SPRT_SC_AIO_MAX			43
#define __SPRT_SC_AIO_PRIO_DELTA_MAX		44
#define __SPRT_SC_DELAYTIMER_MAX		45
#define __SPRT_SC_MQ_OPEN_MAX			46
#define __SPRT_SC_MAPPED_FILES		47	/* swap __SPRT_SC_PAGESIZE vs. BSD */
#define __SPRT_SC_RTSIG_MAX			48
#define __SPRT_SC_SEM_NSEMS_MAX		49
#define __SPRT_SC_SEM_VALUE_MAX		50
#define __SPRT_SC_SIGQUEUE_MAX		51
#define __SPRT_SC_TIMER_MAX			52
#define __SPRT_SC_NPROCESSORS_CONF		57
#define __SPRT_SC_NPROCESSORS_ONLN		58
#define __SPRT_SC_2_PBS			59
#define __SPRT_SC_2_PBS_ACCOUNTING		60
#define __SPRT_SC_2_PBS_CHECKPOINT		61
#define __SPRT_SC_2_PBS_LOCATE		62
#define __SPRT_SC_2_PBS_MESSAGE		63
#define __SPRT_SC_2_PBS_TRACK			64
#define __SPRT_SC_ADVISORY_INFO		65
#define __SPRT_SC_BARRIERS			66
#define __SPRT_SC_CLOCK_SELECTION		67
#define __SPRT_SC_CPUTIME			68
#define __SPRT_SC_FILE_LOCKING		69
#define __SPRT_SC_GETGR_R_SIZE_MAX		70
#define __SPRT_SC_GETPW_R_SIZE_MAX		71
#define __SPRT_SC_HOST_NAME_MAX		72
#define __SPRT_SC_LOGIN_NAME_MAX		73
#define __SPRT_SC_MONOTONIC_CLOCK		74
#define __SPRT_SC_MQ_PRIO_MAX			75
#define __SPRT_SC_READER_WRITER_LOCKS		76
#define __SPRT_SC_REGEXP			77
#define __SPRT_SC_SHELL			78
#define __SPRT_SC_SPAWN			79
#define __SPRT_SC_SPIN_LOCKS			80
#define __SPRT_SC_SPORADIC_SERVER		81
#define __SPRT_SC_THREAD_ATTR_STACKADDR	82
#define __SPRT_SC_THREAD_ATTR_STACKSIZE	83
#define __SPRT_SC_THREAD_CPUTIME		84
#define __SPRT_SC_THREAD_DESTRUCTOR_ITERATIONS 85
#define __SPRT_SC_THREAD_KEYS_MAX		86
#define __SPRT_SC_THREAD_PRIO_INHERIT		87
#define __SPRT_SC_THREAD_PRIO_PROTECT		88
#define __SPRT_SC_THREAD_PRIORITY_SCHEDULING	89
#define __SPRT_SC_THREAD_PROCESS_SHARED	90
#define __SPRT_SC_THREAD_SAFE_FUNCTIONS	91
#define __SPRT_SC_THREAD_SPORADIC_SERVER	92
#define __SPRT_SC_THREAD_STACK_MIN		93
#define __SPRT_SC_THREAD_THREADS_MAX		94
#define __SPRT_SC_TIMEOUTS			95
#define __SPRT_SC_THREADS			96
#define __SPRT_SC_TRACE			97
#define __SPRT_SC_TRACE_EVENT_FILTER		98
#define __SPRT_SC_TRACE_INHERIT		99
#define __SPRT_SC_TRACE_LOG			100
#define __SPRT_SC_TTY_NAME_MAX		101
#define __SPRT_SC_TYPED_MEMORY_OBJECTS	102
#define __SPRT_SC_V6_ILP32_OFF32		103
#define __SPRT_SC_V6_ILP32_OFFBIG		104
#define __SPRT_SC_V6_LP64_OFF64		105
#define __SPRT_SC_V6_LPBIG_OFFBIG		106
#define __SPRT_SC_IPV6			118
#define __SPRT_SC_RAW_SOCKETS			119
#define __SPRT_SC_SYMLOOP_MAX			120
#define __SPRT_SC_ATEXIT_MAX			107
#define __SPRT_SC_IOV_MAX			56
#define __SPRT_SC_PAGE_SIZE			__SPRT_SC_PAGESIZE
#define __SPRT_SC_XOPEN_CRYPT			108
#define __SPRT_SC_XOPEN_ENH_I18N		109
#define __SPRT_SC_XOPEN_LEGACY		110      /* Issue 6 */
#define __SPRT_SC_XOPEN_REALTIME		111      /* Issue 6 */
#define __SPRT_SC_XOPEN_REALTIME_THREADS	112  /* Issue 6 */
#define __SPRT_SC_XOPEN_SHM			113
#define __SPRT_SC_XOPEN_STREAMS		114      /* Issue 6 */
#define __SPRT_SC_XOPEN_UNIX			115
#define __SPRT_SC_XOPEN_VERSION		116
#define __SPRT_SC_XOPEN_XCU_VERSION		121
#define __SPRT_SC_XBS5_ILP32_OFF32		122
#define __SPRT_SC_XBS5_ILP32_OFFBIG		123
#define __SPRT_SC_XBS5_LP64_OFF64		124
#define __SPRT_SC_XBS5_LPBIG_OFFBIG		125
#define __SPRT_SC_SS_REPL_MAX			126
#define __SPRT_SC_TRACE_EVENT_NAME_MAX	127
#define __SPRT_SC_TRACE_NAME_MAX		128
#define __SPRT_SC_TRACE_SYS_MAX		129
#define __SPRT_SC_TRACE_USER_EVENT_MAX	130
#define __SPRT_SC_PASS_MAX			131
#define __SPRT_SC_PHYS_PAGES			200

#define __SPRT_PC_LINK_MAX             1
#define __SPRT_PC_MAX_CANON            2
#define __SPRT_PC_MAX_INPUT            3
#define __SPRT_PC_NAME_MAX             4
#define __SPRT_PC_PATH_MAX             5
#define __SPRT_PC_PIPE_BUF             6
#define __SPRT_PC_CHOWN_RESTRICTED     7
#define __SPRT_PC_NO_TRUNC             8
#define __SPRT_PC_VDISABLE             9

#define __SPRT_PC_NAME_CHARS_MAX       10
#define __SPRT_PC_CASE_SENSITIVE               11
#define __SPRT_PC_CASE_PRESERVING              12
#define __SPRT_PC_EXTENDED_SECURITY_NP        13
#define __SPRT_PC_AUTH_OPAQUE_NP      14

#define __SPRT_PC_2_SYMLINKS          15      /* Symlink supported in directory */
#define __SPRT_PC_ALLOC_SIZE_MIN      16      /* Minimum storage actually allocated */
#define __SPRT_PC_ASYNC_IO            17      /* Async I/O [AIO] supported? */
#define __SPRT_PC_FILESIZEBITS        18      /* # of bits to represent file size */
#define __SPRT_PC_PRIO_IO             19      /* Priority I/O [PIO] supported? */
#define __SPRT_PC_REC_INCR_XFER_SIZE  20      /* Recommended increment for next two */
#define __SPRT_PC_REC_MAX_XFER_SIZE   21      /* Recommended max file transfer size */
#define __SPRT_PC_REC_MIN_XFER_SIZE   22      /* Recommended min file transfer size */
#define __SPRT_PC_REC_XFER_ALIGN      23      /* Recommended buffer alignment */
#define __SPRT_PC_SYMLINK_MAX         24      /* Max # of bytes in symlink name */
#define __SPRT_PC_SYNC_IO             25      /* Sync I/O [SIO] supported? */
#define __SPRT_PC_XATTR_SIZE_BITS     26      /* # of bits to represent maximum xattr size */
#define __SPRT_PC_MIN_HOLE_SIZE       27      /* Recommended minimum hole size for sparse files */
