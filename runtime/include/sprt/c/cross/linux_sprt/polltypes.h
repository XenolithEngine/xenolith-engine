struct __SPRT_POLLFD_NAME {
	int fd; // file descriptor
	short events; // requested events
	short revents; // returned events
};

typedef unsigned long __SPRT_ID(nfds_t);

// clang-format off
#define __SPRT_POLLIN     0x001
#define __SPRT_POLLPRI    0x002
#define __SPRT_POLLOUT    0x004
#define __SPRT_POLLERR    0x008
#define __SPRT_POLLHUP    0x010
#define __SPRT_POLLNVAL   0x020
#define __SPRT_POLLRDNORM 0x040
#define __SPRT_POLLRDBAND 0x080
#define __SPRT_POLLWRNORM 0x100
#define __SPRT_POLLWRBAND 0x200
#define __SPRT_POLLMSG    0x400
#define __SPRT_POLLRDHUP  0x2000
// clang-format on
