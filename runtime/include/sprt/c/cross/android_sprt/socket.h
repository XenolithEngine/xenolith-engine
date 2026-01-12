#if !defined(__LP64__)
typedef __SPRT_ID(int32_t) socklen_t;
#else
typedef __SPRT_ID(uint32_t) socklen_t;
#endif

struct __SPRT_SOCKADDR_NAME {
	sa_family_t sa_family;
	char sa_data[14];
};
