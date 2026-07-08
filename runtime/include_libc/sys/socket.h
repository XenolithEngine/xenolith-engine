/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_SOCKET_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_SOCKET_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/socket.h>

#else

#include <sprt/c/sys/__sprt_socket.h>
#include <sprt/c/bits/iovec.h>

typedef __SPRT_ID(socklen_t) socklen_t;
typedef __SPRT_ID(sa_family_t) sa_family_t;

#if SPRT_WASM

// fd_set + FD_*/FD_SETSIZE. POSIX keeps these in <sys/select.h>, but a lot of BSD-
// derived socket code (e.g. curl's cshutdn.c) uses FD_SET/FD_SETSIZE having included
// only <sys/socket.h>, mirroring the glibc header layout - so surface them here too.
#include <sys/select.h>

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(ssize_t) ssize_t;

struct linger {
	int l_onoff; // on/off switch
	int l_linger; // linger time (seconds)
};

struct msghdr {
	void *msg_name; // optional address
	socklen_t msg_namelen; // size of address
	struct __SPRT_IOVEC_NAME *msg_iov; // scatter/gather array
	int msg_iovlen; // members in msg_iov
	void *msg_control; // ancillary data
	socklen_t msg_controllen; // ancillary data buffer length
	int msg_flags; // flags on received message
};

struct cmsghdr {
	socklen_t cmsg_len; // data byte count, including the cmsghdr
	int cmsg_level; // originating protocol
	int cmsg_type; // protocol-specific type
};

// Linux scatter batch message (sendmmsg/recvmmsg)
struct mmsghdr {
	struct msghdr msg_hdr; // message header
	unsigned int msg_len; // number of bytes transmitted
};

// clang-format off
#define CMSG_DATA(cmsg) ((unsigned char *)(((struct cmsghdr *)(cmsg)) + 1))
#define CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & (size_t) ~(sizeof(size_t) - 1))
#define CMSG_SPACE(len) (CMSG_ALIGN(len) + CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_LEN(len)   (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
#define CMSG_FIRSTHDR(mhdr) \
	((size_t)(mhdr)->msg_controllen >= sizeof(struct cmsghdr) \
			? (struct cmsghdr *)(mhdr)->msg_control \
			: (struct cmsghdr *)0)
#define CMSG_NXTHDR(mhdr, cmsg) __cmsg_nxthdr(mhdr, cmsg)

// SO_* option-value structure used with SCM_RIGHTS/SO_PEERCRED
#define SCM_RIGHTS 0x01
#define SCM_CREDENTIALS 0x02
// clang-format on

struct ucred {
	__SPRT_ID(pid_t) pid;
	__SPRT_ID(uid_t) uid;
	__SPRT_ID(gid_t) gid;
};

__SPRT_BEGIN_DECL

struct cmsghdr *__cmsg_nxthdr(struct msghdr *__mhdr, struct cmsghdr *__cmsg);

int socket(int __domain, int __type, int __protocol);
int socketpair(int __domain, int __type, int __protocol, int __sv[2]);
int bind(int __fd, const struct __SPRT_SOCKADDR_NAME *__addr, socklen_t __len);
int connect(int __fd, const struct __SPRT_SOCKADDR_NAME *__addr, socklen_t __len);
int listen(int __fd, int __backlog);
int accept(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len);
int accept4(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len, int __flags);
int getsockname(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len);
int getpeername(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len);
int shutdown(int __fd, int __how);

int getsockopt(int __fd, int __level, int __optname, void *__SPRT_RESTRICT __optval,
		socklen_t *__SPRT_RESTRICT __optlen);
int setsockopt(int __fd, int __level, int __optname, const void *__optval, socklen_t __optlen);

ssize_t send(int __fd, const void *__buf, size_t __n, int __flags);
ssize_t recv(int __fd, void *__buf, size_t __n, int __flags);
ssize_t sendto(int __fd, const void *__buf, size_t __n, int __flags,
		const struct __SPRT_SOCKADDR_NAME *__addr, socklen_t __addr_len);
ssize_t recvfrom(int __fd, void *__SPRT_RESTRICT __buf, size_t __n, int __flags,
		struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr, socklen_t *__SPRT_RESTRICT __addr_len);
ssize_t sendmsg(int __fd, const struct msghdr *__message, int __flags);
ssize_t recvmsg(int __fd, struct msghdr *__message, int __flags);

struct timespec; // forward decl for recvmmsg timeout (real def via <time.h>)
int sendmmsg(int __fd, struct mmsghdr *__msgvec, unsigned int __vlen, unsigned int __flags);
int recvmmsg(int __fd, struct mmsghdr *__msgvec, unsigned int __vlen, unsigned int __flags,
		struct timespec *__timeout);

__SPRT_END_DECL

#endif // SPRT_WASM

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_SOCKET_H_
