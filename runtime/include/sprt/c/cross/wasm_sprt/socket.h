typedef __SPRT_ID(uint32_t) __SPRT_ID(socklen_t);
typedef unsigned short __SPRT_ID(sa_family_t);

struct __SPRT_SOCKADDR_NAME {
	__SPRT_ID(sa_family_t) sa_family;
	char sa_data[14];
};

typedef int SOCKET;

typedef __SPRT_ID(uint16_t) __SPRT_ID(in_port_t);
typedef __SPRT_ID(uint32_t) __SPRT_ID(in_addr_t);

struct __SPRT_IN_ADDR_NAME {
	__SPRT_ID(in_addr_t) s_addr;
};

// IPv4/IPv6 address + socket-address structures (glibc/musl POSIX layout). These
// complete the forward declarations in cross/__sprt_socket.h and are the concrete
// types behind <netinet/in.h>. sockaddr_storage is large enough to hold any of them.
struct __SPRT_IN6_ADDR_NAME {
	union {
		unsigned char __s6_addr[16];
		__SPRT_ID(uint16_t) __s6_addr16[8];
		__SPRT_ID(uint32_t) __s6_addr32[4];
	} __in6_u;
};
#define s6_addr   __in6_u.__s6_addr
#define s6_addr16 __in6_u.__s6_addr16
#define s6_addr32 __in6_u.__s6_addr32

struct __SPRT_SOCKADDR_IN_NAME {
	__SPRT_ID(sa_family_t) sin_family;
	__SPRT_ID(in_port_t) sin_port;
	struct __SPRT_IN_ADDR_NAME sin_addr;
	unsigned char sin_zero[8];
};

struct __SPRT_SOCKADDR_IN6_NAME {
	__SPRT_ID(sa_family_t) sin6_family;
	__SPRT_ID(in_port_t) sin6_port;
	__SPRT_ID(uint32_t) sin6_flowinfo;
	struct __SPRT_IN6_ADDR_NAME sin6_addr;
	__SPRT_ID(uint32_t) sin6_scope_id;
};

struct sockaddr_storage {
	__SPRT_ID(sa_family_t) ss_family;
	char __ss_padding[128 - sizeof(long) - sizeof(__SPRT_ID(sa_family_t))];
	unsigned long __ss_align;
};

// --- __SPRT_-prefixed socket constants (musl/GNU values; see linux_sprt/socket.h) --
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
#define __SPRT_AF_INET6       10
#define __SPRT_SOL_SOCKET     1
#define __SPRT_SO_REUSEADDR   2
#define __SPRT_SO_TYPE        3
#define __SPRT_SO_ERROR       4
#define __SPRT_SO_DONTROUTE   5
#define __SPRT_SO_BROADCAST   6
#define __SPRT_SO_SNDBUF      7
#define __SPRT_SO_RCVBUF      8
#define __SPRT_SO_KEEPALIVE   9
#define __SPRT_SO_OOBINLINE   10
#define __SPRT_SO_LINGER      13
#define __SPRT_SO_REUSEPORT   15
#define __SPRT_MSG_OOB        0x0001
#define __SPRT_MSG_PEEK       0x0002
#define __SPRT_MSG_DONTROUTE  0x0004
#define __SPRT_MSG_CTRUNC     0x0008
#define __SPRT_MSG_TRUNC      0x0020
#define __SPRT_MSG_DONTWAIT   0x0040
#define __SPRT_MSG_EOR        0x0080
#define __SPRT_MSG_WAITALL    0x0100
#define __SPRT_MSG_NOSIGNAL   0x4000
#define __SPRT_SOMAXCONN      128
// clang-format on

// --- message / control structs (musl/GNU layout) ---------------------------------

struct __SPRT_LINGER_NAME {
	int l_onoff;
	int l_linger;
};

struct __SPRT_MSGHDR_NAME {
	void *msg_name;
	__SPRT_ID(socklen_t) msg_namelen;
	struct __SPRT_IOVEC_NAME *msg_iov;
	__SPRT_ID(size_t) msg_iovlen;
	void *msg_control;
	__SPRT_ID(size_t) msg_controllen;
	int msg_flags;
};

struct __SPRT_CMSGHDR_NAME {
	__SPRT_ID(size_t) cmsg_len;
	int cmsg_level;
	int cmsg_type;
};

struct __SPRT_MMSGHDR_NAME {
	struct __SPRT_MSGHDR_NAME msg_hdr;
	unsigned int msg_len;
};

struct __SPRT_ID(ucred) {
	__SPRT_ID(pid_t) pid;
	__SPRT_ID(uid_t) uid;
	__SPRT_ID(gid_t) gid;
};

#define __SPRT_CMSG_ALIGN(len) \
	(((len) + sizeof(__SPRT_ID(size_t)) - 1) & (__SPRT_ID(size_t)) ~(sizeof(__SPRT_ID(size_t)) - 1))
#define __SPRT_CMSG_DATA(cmsg) ((unsigned char *)((struct __SPRT_CMSGHDR_NAME *)(cmsg) + 1))
#define __SPRT_CMSG_SPACE(len) \
	(__SPRT_CMSG_ALIGN(len) + __SPRT_CMSG_ALIGN(sizeof(struct __SPRT_CMSGHDR_NAME)))
#define __SPRT_CMSG_LEN(len) (__SPRT_CMSG_ALIGN(sizeof(struct __SPRT_CMSGHDR_NAME)) + (len))
#define __SPRT_CMSG_FIRSTHDR(mhdr) \
	((__SPRT_ID(size_t))(mhdr)->msg_controllen >= sizeof(struct __SPRT_CMSGHDR_NAME) \
					? (struct __SPRT_CMSGHDR_NAME *)(mhdr)->msg_control \
					: (struct __SPRT_CMSGHDR_NAME *)0)
