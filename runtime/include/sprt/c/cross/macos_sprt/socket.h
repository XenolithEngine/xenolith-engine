typedef __SPRT_ID(uint32_t) __SPRT_ID(socklen_t);
typedef __SPRT_ID(uint8_t) __SPRT_ID(sa_family_t);

struct __SPRT_SOCKADDR_NAME {
	__SPRT_ID(uint8_t) sa_len; /* total length */
	__SPRT_ID(sa_family_t) sa_family; /* [XSI] address family */
	char sa_data[14]; /* [XSI] addr value */
};

typedef __SPRT_ID(uint32_t) __SPRT_ID(in_addr_t);
typedef __SPRT_ID(uint16_t) __SPRT_ID(in_port_t);

struct __SPRT_IN_ADDR_NAME {
	__SPRT_ID(in_addr_t) s_addr;
};

struct __SPRT_IN6_ADDR_NAME {
	union {
		unsigned char __s6_addr[16];
		__SPRT_ID(uint16_t) __s6_addr16[8];
		__SPRT_ID(uint32_t) __s6_addr32[4];
	} __u6_addr;
};
#if !(defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1)
#define s6_addr   __u6_addr.__s6_addr
#define s6_addr16 __u6_addr.__s6_addr16
#define s6_addr32 __u6_addr.__s6_addr32
#endif

struct __SPRT_SOCKADDR_IN_NAME {
	__SPRT_ID(uint8_t) sin_len;
	__SPRT_ID(sa_family_t) sin_family;
	__SPRT_ID(in_port_t) sin_port;
	struct __SPRT_IN_ADDR_NAME sin_addr;
	unsigned char sin_zero[8];
};

struct __SPRT_SOCKADDR_IN6_NAME {
	__SPRT_ID(uint8_t) sin6_len;
	__SPRT_ID(sa_family_t) sin6_family;
	__SPRT_ID(in_port_t) sin6_port;
	__SPRT_ID(uint32_t) sin6_flowinfo;
	struct __SPRT_IN6_ADDR_NAME sin6_addr;
	__SPRT_ID(uint32_t) sin6_scope_id;
};

typedef int SOCKET;
typedef void sockdata_t;
typedef __SPRT_ID(ssize_t) socksize_t;

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
