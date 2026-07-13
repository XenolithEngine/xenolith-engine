struct __SPRT_POLLFD_NAME {
	int fd; // file descriptor
	short events; // requested events
	short revents; // returned events
};

typedef unsigned int __SPRT_ID(nfds_t);

// clang-format off
#define __SPRT_POLLIN     0x0001
#define __SPRT_POLLPRI    0x0002
#define __SPRT_POLLOUT    0x0004
#define __SPRT_POLLRDNORM 0x0040
#define __SPRT_POLLWRNORM 0x0004
#define __SPRT_POLLRDBAND 0x0080
#define __SPRT_POLLWRBAND 0x0100
#define __SPRT_POLLERR    0x0008
#define __SPRT_POLLHUP    0x0010
#define __SPRT_POLLNVAL   0x0020
// clang-format on
