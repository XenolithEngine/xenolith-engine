// Embox's struct pollfd is the POSIX three-field one, so only the flag values
// diverge - but they diverge badly: POLLOUT and POLLPRI swap places relative to
// Linux, and the priority bands are collapsed onto the plain events (POLLRDNORM
// is POLLIN, POLLWRNORM/POLLWRBAND are POLLOUT, POLLRDBAND is POLLPRI). Those
// aliases are the platform's, not an approximation made here.
//
// poll() gets events/revents forwarded untouched, and outside __SPRT_BUILD these
// macros ARE the application's POLL*, so a Linux table here would silently ask
// for the wrong events (Linux POLLOUT is 4, which is Embox's POLLPRI).

struct __SPRT_POLLFD_NAME {
	int fd; // file descriptor
	short events; // requested events
	short revents; // returned events
};

typedef unsigned long __SPRT_ID(nfds_t);

// clang-format off
#define __SPRT_POLLIN     0x01
#define __SPRT_POLLRDNORM 0x01
#define __SPRT_POLLOUT    0x02 // Linux: 0x004
#define __SPRT_POLLWRNORM 0x02
#define __SPRT_POLLWRBAND 0x02
#define __SPRT_POLLPRI    0x04 // Linux: 0x002
#define __SPRT_POLLRDBAND 0x04
#define __SPRT_POLLERR    0x08
#define __SPRT_POLLHUP    0x10
#define __SPRT_POLLNVAL   0x20
#define __SPRT_POLLRDHUP  0x2000
// __SPRT_POLLMSG is deliberately left undefined - Embox has no POLLMSG, and both
// include_libc/poll.h and the wrapper's assert key off #ifdef.
// clang-format on
