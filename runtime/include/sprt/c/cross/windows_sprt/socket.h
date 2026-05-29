typedef int __SPRT_ID(socklen_t);

typedef unsigned short ADDRESS_FAMILY;
typedef ADDRESS_FAMILY __SPRT_ID(sa_family_t);

struct __SPRT_SOCKADDR_NAME {
	ADDRESS_FAMILY sa_family;
	char sa_data[14];
};

typedef unsigned long long SOCKET;

typedef __SPRT_ID(uint16_t) __SPRT_ID(in_port_t);
typedef __SPRT_ID(uint32_t) __SPRT_ID(in_addr_t);

typedef struct __SPRT_IN_ADDR_NAME {
	union {
		struct {
			unsigned char s_b1, s_b2, s_b3, s_b4;
		} S_un_b;
		struct {
			unsigned short s_w1, s_w2;
		} S_un_w;
		unsigned long S_addr;
	} S_un;

} IN_ADDR, *PIN_ADDR, *LPIN_ADDR;

typedef struct __SPRT_IN6_ADDR_NAME {
	union {
		unsigned char Byte[16];
		unsigned short Word[8];
	} u;
} IN6_ADDR, *PIN6_ADDR, *LPIN6_ADDR;

#define s_addr  S_un.S_addr /* can be used for most tcp & ip code */
#define s_host  S_un.S_un_b.s_b2    // host on imp
#define s_net   S_un.S_un_b.s_b1    // network
#define s_imp   S_un.S_un_w.s_w2    // imp
#define s_impno S_un.S_un_b.s_b4    // imp #
#define s_lh    S_un.S_un_b.s_b3    // logical host

typedef struct __SPRT_SOCKADDR_IN_NAME {
	ADDRESS_FAMILY sin_family;
	unsigned short sin_port;
	IN_ADDR sin_addr;
	char sin_zero[8];
} SOCKADDR_IN, *PSOCKADDR_IN;

typedef struct {
	union {
		struct {
			unsigned long Zone	: 28;
			unsigned long Level : 4;
		} DUMMYSTRUCTNAME;
		unsigned long Value;
	} DUMMYUNIONNAME;
} SCOPE_ID, *PSCOPE_ID;

typedef struct __SPRT_SOCKADDR_IN6_NAME {
	ADDRESS_FAMILY sin6_family; // AF_INET6.
	unsigned short sin6_port; // Transport level port number.
	unsigned long sin6_flowinfo; // IPv6 flow information.
	IN6_ADDR sin6_addr; // IPv6 address.
	union {
		unsigned long sin6_scope_id; // Set of interfaces for a scope.
		SCOPE_ID sin6_scope_struct;
	};
} SOCKADDR_IN6, *PSOCKADDR_IN6, *LPSOCKADDR_IN6;

#define _S6_un      u
#define _S6_u8      Byte
#define s6_addr     _S6_un._S6_u8

struct hostent {
	char *h_name; /* official name of host */
	char **h_aliases; /* alias list */
	short h_addrtype; /* host address type */
	short h_length; /* length of address */
	char **h_addr_list; /* list of addresses */
#define h_addr  h_addr_list[0]          /* address, for backward compat */
};

struct netent {
	char *n_name; /* official name of net */
	char **n_aliases; /* alias list */
	short n_addrtype; /* net address type */
	unsigned long n_net; /* network # */
};

struct servent {
	char *s_name; /* official service name */
	char **s_aliases; /* alias list */
	char *s_proto; /* protocol to use */
	short s_port; /* port # */
};

struct protoent {
	char *p_name; /* official protocol name */
	char **p_aliases; /* alias list */
	short p_proto; /* protocol # */
};

typedef struct addrinfo {
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	size_t ai_addrlen;
	char *ai_canonname;
	struct sockaddr *ai_addr;
	struct addrinfo *ai_next;
} ADDRINFOA, *PADDRINFOA;

#define WSADESCRIPTION_LEN      256
#define WSASYS_STATUS_LEN       128

typedef struct WSAData {
	unsigned long wVersion;
	unsigned long wHighVersion;
	unsigned short iMaxSockets;
	unsigned short iMaxUdpDg;
	char *lpVendorInfo;
	char szDescription[WSADESCRIPTION_LEN + 1];
	char szSystemStatus[WSASYS_STATUS_LEN + 1];
} WSADATA, *LPWSADATA;

#define _SS_MAXSIZE 128
#define _SS_ALIGNSIZE (sizeof( __SPRT_ID(int64_t)))
#define _SS_PAD1SIZE (_SS_ALIGNSIZE - sizeof(unsigned short))
#define _SS_PAD2SIZE (_SS_MAXSIZE - (sizeof(unsigned short) + _SS_PAD1SIZE + _SS_ALIGNSIZE))

typedef struct sockaddr_storage {
	ADDRESS_FAMILY ss_family;
	char __ss_pad1[_SS_PAD1SIZE];
	__SPRT_ID(int64_t) __ss_align;
	char __ss_pad2[_SS_PAD2SIZE];
} SOCKADDR_STORAGE_LH, *PSOCKADDR_STORAGE_LH, *LPSOCKADDR_STORAGE_LH;
