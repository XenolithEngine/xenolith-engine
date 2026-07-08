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
