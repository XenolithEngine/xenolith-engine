typedef __SPRT_ID(uint32_t) __SPRT_ID(socklen_t);
typedef __SPRT_ID(uint8_t) __SPRT_ID(sa_family_t);

struct __SPRT_SOCKADDR_NAME {
	__SPRT_ID(uint8_t) sa_len; /* total length */
	__SPRT_ID(socklen_t) sa_family; /* [XSI] address family */
	char sa_data[14]; /* [XSI] addr value */
};
