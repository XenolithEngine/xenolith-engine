#if !defined(__LP64__)
typedef __SPRT_ID(int32_t) __SPRT_ID(socklen_t);
#else
typedef __SPRT_ID(uint32_t) __SPRT_ID(socklen_t);
#endif

typedef unsigned short __SPRT_ID(sa_family_t);

struct __SPRT_SOCKADDR_NAME {
	__SPRT_ID(sa_family_t) sa_family;
	char sa_data[14];
};
