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

// --- __SPRT_-prefixed socket constants (glibc values) ------------------------------
// The internal source of truth for the portable socket constants. Unlike the plain
// SOCK_*/AF_*/SO_*/... in sockdef.h (which collide with the native <sys/socket.h> and so
// are freestanding-only), these are namespaced and safe to define everywhere, so the
// plain names alias them and SPRuntimeCSysSocket.cpp static_asserts each == the native.
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

// --- message / control structs (glibc GNU layout: size_t length fields) ----------

struct __SPRT_LINGER_NAME {
	int l_onoff; // on/off switch
	int l_linger; // linger time (seconds)
};

struct __SPRT_MSGHDR_NAME {
	void *msg_name; // optional address
	__SPRT_ID(socklen_t) msg_namelen; // size of address
	struct __SPRT_IOVEC_NAME *msg_iov; // scatter/gather array
	__SPRT_ID(size_t) msg_iovlen; // members in msg_iov
	void *msg_control; // ancillary data
	__SPRT_ID(size_t) msg_controllen; // ancillary data buffer length
	int msg_flags; // flags on received message
};

struct __SPRT_CMSGHDR_NAME {
	__SPRT_ID(size_t) cmsg_len; // data byte count, including the cmsghdr
	int cmsg_level; // originating protocol
	int cmsg_type; // protocol-specific type
};

struct __SPRT_MMSGHDR_NAME {
	struct __SPRT_MSGHDR_NAME msg_hdr; // message header
	unsigned int msg_len; // number of received/sent bytes
};

struct __SPRT_ID(ucred) {
	__SPRT_ID(pid_t) pid;
	__SPRT_ID(uid_t) uid;
	__SPRT_ID(gid_t) gid;
};

// CMSG_* helpers (glibc): align to sizeof(size_t). Public names are mapped from these
// in include_libc/sys/socket.h (kept __SPRT-prefixed here to avoid clashing with the
// native <sys/socket.h> macros on hosted targets).
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
