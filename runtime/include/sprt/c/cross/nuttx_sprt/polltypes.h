// NuttX's struct pollfd is not the POSIX three-field one: events/revents are
// uint32_t rather than short, and poll() uses three trailing driver-private fields
// that it writes through. The wrapper forwards the caller's array by a plain cast
// (and outside __SPRT_BUILD this struct IS the application's struct pollfd), so the
// whole record - trailing fields included - has to match NuttX's.
//
// NuttX also collapses the priority bands: POLLRDNORM/POLLRDBAND are POLLIN and
// POLLWRNORM/POLLWRBAND are POLLOUT, and POLLRDHUP is POLLHUP ("NuttX does not
// support shutdown(fd, SHUT_RD)"). Those aliases are the platform's, not an
// approximation made here.

#include <sprt/c/bits/__sprt_uint32_t.h>

struct __SPRT_POLLFD_NAME {
	int fd; // file descriptor
	__SPRT_ID(uint32_t) events; // requested events (NuttX pollevent_t)
	__SPRT_ID(uint32_t) revents; // returned events

	// Written by NuttX's poll() implementation; present here only to keep the
	// record the right size. Declared as raw pointers because sprt headers cannot
	// name the FAR / CODE qualifiers or the pollcb_t signature.
	void *__arg; // poll callback argument
	void *__cb; // poll callback (pollcb_t)
	void *__priv; // driver-private
};

typedef unsigned int __SPRT_ID(nfds_t);

// clang-format off
#define __SPRT_POLLIN     0x01
#define __SPRT_POLLRDNORM 0x01
#define __SPRT_POLLRDBAND 0x01
#define __SPRT_POLLPRI    0x02
#define __SPRT_POLLOUT    0x04
#define __SPRT_POLLWRNORM 0x04
#define __SPRT_POLLWRBAND 0x04
#define __SPRT_POLLERR    0x08
#define __SPRT_POLLHUP    0x10
#define __SPRT_POLLRDHUP  0x10
#define __SPRT_POLLNVAL   0x20
// __SPRT_POLLMSG is deliberately left undefined - NuttX has no POLLMSG, and both
// include_libc/poll.h and the wrapper's assert key off #ifdef.
// clang-format on
