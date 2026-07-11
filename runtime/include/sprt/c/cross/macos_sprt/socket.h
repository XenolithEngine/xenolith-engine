typedef __SPRT_ID(uint32_t) __SPRT_ID(socklen_t);
typedef __SPRT_ID(uint8_t) __SPRT_ID(sa_family_t);

struct __SPRT_SOCKADDR_NAME {
	__SPRT_ID(uint8_t) sa_len; /* total length */
	__SPRT_ID(sa_family_t) sa_family; /* [XSI] address family */
	char sa_data[14]; /* [XSI] addr value */
};

typedef __SPRT_ID(uint32_t) __SPRT_ID(in_addr_t);

typedef int SOCKET;

// --- __SPRT_-prefixed socket constants (Darwin BSD values) -------------------------
// SOCK_CLOEXEC/NONBLOCK have no macOS native (accept4 is emulated); the values are the
// SPRT convention and are not asserted on macOS (guarded by #ifdef in the wrapper).
// clang-format off
#define __SPRT_SHUT_RD        0
#define __SPRT_SHUT_WR        1
#define __SPRT_SHUT_RDWR      2
#define __SPRT_SOCK_STREAM    1
#define __SPRT_SOCK_DGRAM     2
#define __SPRT_SOCK_RAW       3
#define __SPRT_SOCK_SEQPACKET 5
#define __SPRT_SOCK_CLOEXEC   02000000
#define __SPRT_SOCK_NONBLOCK  04000
#define __SPRT_AF_UNSPEC      0
#define __SPRT_AF_UNIX        1
#define __SPRT_AF_INET        2
#define __SPRT_AF_INET6       30
#define __SPRT_SOL_SOCKET     0xffff
#define __SPRT_SO_REUSEADDR   0x0004
#define __SPRT_SO_TYPE        0x1008
#define __SPRT_SO_ERROR       0x1007
#define __SPRT_SO_DONTROUTE   0x0010
#define __SPRT_SO_BROADCAST   0x0020
#define __SPRT_SO_SNDBUF      0x1001
#define __SPRT_SO_RCVBUF      0x1002
#define __SPRT_SO_KEEPALIVE   0x0008
#define __SPRT_SO_OOBINLINE   0x0100
#define __SPRT_SO_LINGER      0x0080
#define __SPRT_SO_REUSEPORT   0x0200
#define __SPRT_MSG_OOB        0x1
#define __SPRT_MSG_PEEK       0x2
#define __SPRT_MSG_DONTROUTE  0x4
#define __SPRT_MSG_EOR        0x8
#define __SPRT_MSG_TRUNC      0x10
#define __SPRT_MSG_CTRUNC     0x20
#define __SPRT_MSG_WAITALL    0x40
#define __SPRT_MSG_DONTWAIT   0x80
#define __SPRT_MSG_NOSIGNAL   0x80000
#define __SPRT_SOMAXCONN      128
// clang-format on

// --- message / control structs (Darwin BSD layout: int msg_iovlen, socklen_t
// control/cmsg lengths, 32-bit CMSG alignment; no ucred - macOS uses xucred) --------

struct __SPRT_LINGER_NAME {
	int l_onoff;
	int l_linger;
};

struct __SPRT_MSGHDR_NAME {
	void *msg_name;
	__SPRT_ID(socklen_t) msg_namelen;
	struct __SPRT_IOVEC_NAME *msg_iov;
	int msg_iovlen;
	void *msg_control;
	__SPRT_ID(socklen_t) msg_controllen;
	int msg_flags;
};

struct __SPRT_CMSGHDR_NAME {
	__SPRT_ID(socklen_t) cmsg_len;
	int cmsg_level;
	int cmsg_type;
};

struct __SPRT_MMSGHDR_NAME {
	struct __SPRT_MSGHDR_NAME msg_hdr;
	unsigned int msg_len;
};

#define __SPRT_CMSG_ALIGN(len) (((len) + 3) & ~((__SPRT_ID(size_t))3))
#define __SPRT_CMSG_DATA(cmsg) ((unsigned char *)((struct __SPRT_CMSGHDR_NAME *)(cmsg) + 1))
#define __SPRT_CMSG_SPACE(len) \
	(__SPRT_CMSG_ALIGN(len) + __SPRT_CMSG_ALIGN(sizeof(struct __SPRT_CMSGHDR_NAME)))
#define __SPRT_CMSG_LEN(len) (__SPRT_CMSG_ALIGN(sizeof(struct __SPRT_CMSGHDR_NAME)) + (len))
#define __SPRT_CMSG_FIRSTHDR(mhdr) \
	((__SPRT_ID(socklen_t))(mhdr)->msg_controllen >= sizeof(struct __SPRT_CMSGHDR_NAME) \
					? (struct __SPRT_CMSGHDR_NAME *)(mhdr)->msg_control \
					: (struct __SPRT_CMSGHDR_NAME *)0)
