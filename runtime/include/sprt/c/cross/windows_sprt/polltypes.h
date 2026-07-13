typedef unsigned long long SOCKET;
typedef short SHORT;

struct __SPRT_POLLFD_NAME {
	SOCKET fd; // file descriptor
	SHORT events; // requested events
	SHORT revents; // returned events
};

typedef unsigned long __SPRT_ID(nfds_t);

#define __SPRT_POLLRDNORM 0x0100
#define __SPRT_POLLRDBAND 0x0200
#define __SPRT_POLLIN     (__SPRT_POLLRDNORM | __SPRT_POLLRDBAND)
#define __SPRT_POLLPRI    0x0400
#define __SPRT_POLLWRNORM 0x0010
#define __SPRT_POLLOUT    (__SPRT_POLLWRNORM)
#define __SPRT_POLLWRBAND 0x0020
#define __SPRT_POLLERR    0x0001
#define __SPRT_POLLHUP    0x0002
#define __SPRT_POLLNVAL   0x0004
