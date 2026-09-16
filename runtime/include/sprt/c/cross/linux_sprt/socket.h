typedef __SPRT_ID(uint32_t) __SPRT_ID(socklen_t);
typedef unsigned short __SPRT_ID(sa_family_t);

struct __SPRT_SOCKADDR_NAME {
	__SPRT_ID(sa_family_t) sa_family;
	char sa_data[14];
};

struct __SPRT_SOCKADDR_STORAGE_NAME {
	__SPRT_ID(sa_family_t) ss_family;
	char __ss_padding[128-sizeof(long)-sizeof(__SPRT_ID(sa_family_t))];
	unsigned long __ss_align;
};

typedef int SOCKET;
typedef void sockdata_t;
typedef __SPRT_ID(ssize_t) socksize_t;

typedef __SPRT_ID(uint16_t) __SPRT_ID(in_port_t);
typedef __SPRT_ID(uint32_t) __SPRT_ID(in_addr_t);

struct __SPRT_IN_ADDR_NAME {
	__SPRT_ID(in_addr_t) s_addr;
};

struct __SPRT_IN6_ADDR_NAME {
	union {
		unsigned char __s6_addr[16];
		__SPRT_ID(uint16_t) __s6_addr16[8];
		__SPRT_ID(uint32_t) __s6_addr32[4];
	} __in6_u;
};

#if !(defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1)
#define s6_addr   __in6_u.__s6_addr
#define s6_addr16 __in6_u.__s6_addr16
#define s6_addr32 __in6_u.__s6_addr32
#endif

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
