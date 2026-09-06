// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// ---------------------------------------------------------------------------
// +open sysroot <-> real macOS SDK: the POSIX surface.
//
// sprt is not involved here. This probe pins the SDK-free `+open` sysroot --
// assembled from apple-oss checkouts plus a hand-written overlay -- against the
// real Apple SDK, which target-apple/README.md describes as "consulted only as a
// read-only reference for validation". That validation was done by hand, once;
// this is the re-runnable form of it.
//
// The two header sets spell the same names, so they cannot share a translation
// unit the way the check-*.cpp files can. Instead check.sh compiles THIS FILE
// TWICE -- once with -isysroot <SDK>, once against the +open sysroot -- and
// diffs the emitted values. Each entry becomes an `extern const long long`
// global whose initialiser lands in the LLVM IR, so no target code ever runs.
//
// A name present in only one of the two sysroots fails to compile there, which
// is itself a finding: the +open headers must carry Darwin's surface exactly,
// no wider and no narrower.
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <termios.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/event.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <sys/attr.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <mach/mach_time.h>
#include <mach/thread_info.h>

#define V(x)      const long long abi_##x = (long long)(x);
#define S(n, t)   const long long abisz_##n = (long long)sizeof(t);
#define A(n, t)   const long long abial_##n = (long long)_Alignof(t);
#define O(n, t, f) const long long abioff_##n##_##f = (long long)__builtin_offsetof(t, f);

// === errno =================================================================
V(EPERM) V(EINTR) V(EDEADLK) V(EAGAIN) V(ENOTSUP) V(ELOOP) V(EOVERFLOW)
V(ECANCELED) V(EBADMACHO) V(ESHLIBVERS) V(EPWROFF) V(EDEVERR) V(EQFULL) V(ELAST)

// === open()/fcntl() ========================================================
V(O_RDONLY) V(O_WRONLY) V(O_RDWR) V(O_ACCMODE) V(O_NONBLOCK) V(O_APPEND)
V(O_CREAT) V(O_TRUNC) V(O_EXCL) V(O_NOFOLLOW) V(O_DIRECTORY) V(O_SYMLINK)
V(O_CLOEXEC) V(O_EVTONLY) V(O_DSYNC) V(O_SYNC) V(O_SHLOCK) V(O_EXLOCK)
V(F_GETFD) V(F_SETFD) V(F_GETFL) V(F_SETFL) V(F_DUPFD) V(F_DUPFD_CLOEXEC)
V(F_GETLK) V(F_SETLK) V(F_SETLKW) V(F_NOCACHE) V(F_FULLFSYNC) V(F_RDADVISE)
V(F_GETPATH) V(F_PREALLOCATE) V(F_SETSIZE) V(F_LOG2PHYS) V(FD_CLOEXEC)
V(AT_FDCWD) V(AT_EACCESS) V(AT_SYMLINK_NOFOLLOW) V(AT_SYMLINK_FOLLOW) V(AT_REMOVEDIR)

// === signals ===============================================================
V(SIGHUP) V(SIGINT) V(SIGQUIT) V(SIGILL) V(SIGTRAP) V(SIGABRT) V(SIGEMT)
V(SIGFPE) V(SIGKILL) V(SIGBUS) V(SIGSEGV) V(SIGSYS) V(SIGPIPE) V(SIGALRM)
V(SIGTERM) V(SIGURG) V(SIGSTOP) V(SIGTSTP) V(SIGCONT) V(SIGCHLD) V(SIGTTIN)
V(SIGTTOU) V(SIGIO) V(SIGXCPU) V(SIGXFSZ) V(SIGVTALRM) V(SIGPROF) V(SIGWINCH)
V(SIGINFO) V(SIGUSR1) V(SIGUSR2) V(NSIG)
V(SA_ONSTACK) V(SA_RESTART) V(SA_NOCLDSTOP) V(SA_NODEFER) V(SA_RESETHAND) V(SA_SIGINFO)
V(SIG_BLOCK) V(SIG_UNBLOCK) V(SIG_SETMASK)

// === sockets ===============================================================
V(AF_UNSPEC) V(AF_UNIX) V(AF_INET) V(AF_INET6) V(AF_LINK) V(AF_SYSTEM) V(AF_VSOCK) V(AF_MAX)
V(SOCK_STREAM) V(SOCK_DGRAM) V(SOCK_RAW) V(SOCK_SEQPACKET) V(SOCK_RDM)
V(SOL_SOCKET) V(SO_REUSEADDR) V(SO_REUSEPORT) V(SO_KEEPALIVE) V(SO_BROADCAST)
V(SO_LINGER) V(SO_SNDBUF) V(SO_RCVBUF) V(SO_SNDTIMEO) V(SO_RCVTIMEO) V(SO_ERROR)
V(SO_TYPE) V(SO_NOSIGPIPE) V(SO_TIMESTAMP) V(SOMAXCONN)
V(MSG_OOB) V(MSG_PEEK) V(MSG_DONTROUTE) V(MSG_EOR) V(MSG_TRUNC) V(MSG_CTRUNC)
V(MSG_WAITALL) V(MSG_DONTWAIT) V(MSG_NOSIGNAL) V(MSG_HAVEMORE) V(MSG_RCVMORE)
V(SHUT_RD) V(SHUT_WR) V(SHUT_RDWR) V(SCM_RIGHTS) V(SCM_CREDS) V(SCM_TIMESTAMP)

// === netinet ===============================================================
V(IPPROTO_IP) V(IPPROTO_ICMP) V(IPPROTO_TCP) V(IPPROTO_UDP) V(IPPROTO_IPV6)
V(IPPROTO_ICMPV6) V(IPPROTO_RAW) V(IPPROTO_SCTP)
V(IP_TOS) V(IP_TTL) V(IP_HDRINCL) V(IP_MULTICAST_IF) V(IP_MULTICAST_TTL)
V(IP_MULTICAST_LOOP) V(IP_ADD_MEMBERSHIP) V(IP_DROP_MEMBERSHIP)
V(IP_RECVDSTADDR) V(IP_RECVIF) V(IP_PKTINFO)
V(IPV6_V6ONLY) V(IPV6_UNICAST_HOPS) V(IPV6_MULTICAST_IF) V(IPV6_MULTICAST_HOPS)
V(IPV6_JOIN_GROUP) V(IPV6_LEAVE_GROUP) V(IPV6_TCLASS) V(IPV6_RECVTCLASS)
V(TCP_NODELAY) V(TCP_MAXSEG) V(TCP_KEEPALIVE) V(TCP_KEEPINTVL) V(TCP_KEEPCNT)
V(TCP_NOTSENT_LOWAT) V(TCP_FASTOPEN) V(TCP_CONNECTION_INFO)
V(INET_ADDRSTRLEN) V(INET6_ADDRSTRLEN)

// === mmap / madvise ========================================================
V(PROT_NONE) V(PROT_READ) V(PROT_WRITE) V(PROT_EXEC)
V(MAP_SHARED) V(MAP_PRIVATE) V(MAP_FIXED) V(MAP_ANON) V(MAP_NORESERVE) V(MAP_JIT)
V(MS_ASYNC) V(MS_SYNC) V(MS_INVALIDATE)
V(MADV_NORMAL) V(MADV_RANDOM) V(MADV_SEQUENTIAL) V(MADV_WILLNEED)
V(MADV_DONTNEED) V(MADV_FREE) V(MADV_ZERO_WIRED_PAGES)

// === poll / select =========================================================
V(POLLIN) V(POLLPRI) V(POLLOUT) V(POLLERR) V(POLLHUP) V(POLLNVAL)
V(POLLRDNORM) V(POLLWRNORM) V(POLLRDBAND) V(POLLWRBAND) V(FD_SETSIZE)

// === kqueue (the Darwin event backend sprt's dispatch layer uses) ==========
V(EVFILT_READ) V(EVFILT_WRITE) V(EVFILT_AIO) V(EVFILT_VNODE) V(EVFILT_PROC)
V(EVFILT_SIGNAL) V(EVFILT_TIMER) V(EVFILT_MACHPORT) V(EVFILT_USER) V(EVFILT_EXCEPT)
V(EV_ADD) V(EV_DELETE) V(EV_ENABLE) V(EV_DISABLE) V(EV_ONESHOT) V(EV_CLEAR)
V(EV_RECEIPT) V(EV_DISPATCH) V(EV_EOF) V(EV_ERROR)
V(NOTE_DELETE) V(NOTE_WRITE) V(NOTE_EXTEND) V(NOTE_ATTRIB) V(NOTE_LINK)
V(NOTE_RENAME) V(NOTE_REVOKE) V(NOTE_FUNLOCK)
V(NOTE_SECONDS) V(NOTE_USECONDS) V(NOTE_NSECONDS) V(NOTE_MACHTIME)
V(NOTE_ABSOLUTE) V(NOTE_CRITICAL) V(NOTE_LEEWAY) V(NOTE_TRIGGER)

// === clocks / time =========================================================
V(CLOCK_REALTIME) V(CLOCK_MONOTONIC) V(CLOCK_MONOTONIC_RAW)
V(CLOCK_MONOTONIC_RAW_APPROX) V(CLOCK_UPTIME_RAW) V(CLOCK_UPTIME_RAW_APPROX)
V(CLOCK_PROCESS_CPUTIME_ID) V(CLOCK_THREAD_CPUTIME_ID)
V(ITIMER_REAL) V(ITIMER_VIRTUAL) V(ITIMER_PROF)

// === stat / dirent =========================================================
V(S_IFMT) V(S_IFIFO) V(S_IFCHR) V(S_IFDIR) V(S_IFBLK) V(S_IFREG) V(S_IFLNK)
V(S_IFSOCK) V(S_ISUID) V(S_ISGID) V(S_ISVTX) V(S_IRWXU) V(S_IRWXG) V(S_IRWXO)
V(UF_NODUMP) V(UF_IMMUTABLE) V(UF_APPEND) V(UF_OPAQUE) V(UF_HIDDEN)
V(SF_ARCHIVED) V(SF_IMMUTABLE) V(SF_APPEND)
V(DT_UNKNOWN) V(DT_FIFO) V(DT_CHR) V(DT_DIR) V(DT_BLK) V(DT_REG) V(DT_LNK) V(DT_SOCK)
V(MAXNAMLEN) V(MAXPATHLEN) V(PATH_MAX) V(NAME_MAX)

// === resource limits / wait ================================================
V(RLIMIT_CPU) V(RLIMIT_FSIZE) V(RLIMIT_DATA) V(RLIMIT_STACK) V(RLIMIT_CORE)
V(RLIMIT_AS) V(RLIMIT_MEMLOCK) V(RLIMIT_NPROC) V(RLIMIT_NOFILE)
V(WNOHANG) V(WUNTRACED)

// === termios ===============================================================
V(TCSANOW) V(TCSADRAIN) V(TCSAFLUSH) V(VMIN) V(VTIME) V(VEOF) V(VINTR)
V(ICANON) V(ECHO) V(ISIG) V(IEXTEN) V(NCCS) V(B9600) V(B115200)

// === sysconf ===============================================================
V(_SC_PAGESIZE) V(_SC_NPROCESSORS_CONF) V(_SC_NPROCESSORS_ONLN) V(_SC_OPEN_MAX)
V(_SC_CLK_TCK) V(_SC_HOST_NAME_MAX) V(_SC_GETPW_R_SIZE_MAX) V(_SC_PHYS_PAGES)

// === mach ==================================================================
V(THREAD_BASIC_INFO) V(THREAD_BASIC_INFO_COUNT) V(TH_FLAGS_IDLE) V(KERN_SUCCESS)

// === struct layouts ========================================================
S(stat, struct stat) A(stat, struct stat)
O(stat, struct stat, st_dev) O(stat, struct stat, st_mode) O(stat, struct stat, st_nlink)
O(stat, struct stat, st_ino) O(stat, struct stat, st_uid) O(stat, struct stat, st_gid)
O(stat, struct stat, st_rdev) O(stat, struct stat, st_atimespec)
O(stat, struct stat, st_mtimespec) O(stat, struct stat, st_ctimespec)
O(stat, struct stat, st_birthtimespec) O(stat, struct stat, st_size)
O(stat, struct stat, st_blocks) O(stat, struct stat, st_blksize) O(stat, struct stat, st_flags)

S(dirent, struct dirent) A(dirent, struct dirent)
O(dirent, struct dirent, d_ino) O(dirent, struct dirent, d_seekoff)
O(dirent, struct dirent, d_reclen) O(dirent, struct dirent, d_namlen)
O(dirent, struct dirent, d_type) O(dirent, struct dirent, d_name)

S(kevent, struct kevent) A(kevent, struct kevent)
O(kevent, struct kevent, ident) O(kevent, struct kevent, filter)
O(kevent, struct kevent, flags) O(kevent, struct kevent, fflags)
O(kevent, struct kevent, data) O(kevent, struct kevent, udata)
S(kevent64_s, struct kevent64_s)

S(statvfs, struct statvfs) S(statfs, struct statfs)
O(statfs, struct statfs, f_bsize) O(statfs, struct statfs, f_blocks)
O(statfs, struct statfs, f_flags) O(statfs, struct statfs, f_fstypename)
O(statfs, struct statfs, f_mntonname) O(statfs, struct statfs, f_mntfromname)

S(termios, struct termios) O(termios, struct termios, c_cc) O(termios, struct termios, c_ispeed)
S(rlimit, struct rlimit) S(rusage, struct rusage) O(rusage, struct rusage, ru_maxrss)
S(timespec, struct timespec) S(timeval, struct timeval) S(itimerval, struct itimerval)
S(sockaddr, struct sockaddr) S(sockaddr_in, struct sockaddr_in)
S(sockaddr_in6, struct sockaddr_in6) S(sockaddr_un, struct sockaddr_un)
S(sockaddr_storage, struct sockaddr_storage) A(sockaddr_storage, struct sockaddr_storage)
S(msghdr, struct msghdr) O(msghdr, struct msghdr, msg_control) O(msghdr, struct msghdr, msg_flags)
S(cmsghdr, struct cmsghdr) S(iovec, struct iovec) S(linger, struct linger)
S(pollfd, struct pollfd) S(fd_set, fd_set) S(sigaction, struct sigaction)
S(mach_timebase_info_data_t, mach_timebase_info_data_t)
S(thread_basic_info_data_t, thread_basic_info_data_t)

// === scalar typedef widths =================================================
S(off_t, off_t) S(ino_t, ino_t) S(dev_t, dev_t) S(mode_t, mode_t)
S(nlink_t, nlink_t) S(blkcnt_t, blkcnt_t) S(blksize_t, blksize_t)
S(time_t, time_t) S(suseconds_t, suseconds_t) S(clock_t, clock_t)
S(pid_t, pid_t) S(uid_t, uid_t) S(gid_t, gid_t) S(socklen_t, socklen_t)
S(sa_family_t, sa_family_t) S(sigset_t, sigset_t) S(size_t, size_t)
S(ptrdiff_t, ptrdiff_t) S(wchar_t, wchar_t) S(long_double, long double)
S(pthread_t, pthread_t) S(pthread_mutex_t, pthread_mutex_t)
S(pthread_cond_t, pthread_cond_t) S(pthread_rwlock_t, pthread_rwlock_t)
S(pthread_key_t, pthread_key_t) S(pthread_once_t, pthread_once_t)
S(pthread_attr_t, pthread_attr_t) A(pthread_mutex_t, pthread_mutex_t)
