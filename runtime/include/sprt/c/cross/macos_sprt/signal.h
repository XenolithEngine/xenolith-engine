#define __SPRT_SIGHUP  1       /* hangup */
#define __SPRT_SIGINT  2       /* interrupt */
#define __SPRT_SIGQUIT 3       /* quit */
#define __SPRT_SIGILL  4       /* illegal instruction (not reset when caught) */
#define __SPRT_SIGTRAP 5       /* trace trap (not reset when caught) */
#define __SPRT_SIGABRT 6       /* abort() */
#define __SPRT_SIGIOT  __SPRT_SIGABRT /* compatibility */
#define __SPRT_SIGEMT  7       /* EMT instruction */
#define __SPRT_SIGFPE  8       /* floating point exception */
#define __SPRT_SIGKILL 9       /* kill (cannot be caught or ignored) */
#define __SPRT_SIGBUS  10      /* bus error */
#define __SPRT_SIGSEGV 11      /* segmentation violation */
#define __SPRT_SIGSYS  12      /* bad argument to system call */
#define __SPRT_SIGPIPE 13      /* write on a pipe with no one to read it */
#define __SPRT_SIGALRM 14      /* alarm clock */
#define __SPRT_SIGTERM 15      /* software termination signal from kill */
#define __SPRT_SIGURG  16      /* urgent condition on IO channel */
#define __SPRT_SIGSTOP 17      /* sendable stop signal not from tty */
#define __SPRT_SIGTSTP 18      /* stop signal from tty */
#define __SPRT_SIGCONT 19      /* continue a stopped process */
#define __SPRT_SIGCHLD 20      /* to parent on child stop or exit */
#define __SPRT_SIGTTIN 21      /* to readers pgrp upon background tty read */
#define __SPRT_SIGTTOU 22      /* like TTIN for output if (tp->t_local&LTOSTOP) */
#define __SPRT_SIGIO   23      /* input/output possible signal */
#define __SPRT_SIGXCPU 24      /* exceeded CPU time limit */
#define __SPRT_SIGXFSZ 25      /* exceeded file size limit */
#define __SPRT_SIGVTALRM 26    /* virtual time alarm */
#define __SPRT_SIGPROF 27      /* profiling time alarm */
#define __SPRT_SIGWINCH 28     /* window size changes */
#define __SPRT_SIGINFO 29      /* information request */
#define __SPRT_SIGUSR1 30      /* user defined signal 1 */
#define __SPRT_SIGUSR2 31      /* user defined signal 2 */

#define __SPRT__NSIG 32

#define __SPRT_SIG_DFL         (void (*)(int))0
#define __SPRT_SIG_IGN         (void (*)(int))1
#define __SPRT_SIG_HOLD        (void (*)(int))5
#define __SPRT_SIG_ERR         ((void (*)(int))-1)

typedef int __SPRT_ID(sig_atomic_t);

#define __SPRT_SIG_ATOMIC_MIN __SPRT_INT_MAX
#define __SPRT_SIG_ATOMIC_MAX (-1-__SPRT_INT_MAX)

typedef struct __SPRT_ID(__sigset_t) {
	__SPRT_ID(uint32_t) __bits[1];
} __SPRT_ID(sigset_t);
