#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define __SPRT_ISbit(bit)	(1 << (bit))
#else /* __BYTE_ORDER == __LITTLE_ENDIAN */
#define __SPRT_ISbit(bit)	((bit) < 8 ? ((1 << (bit)) << 8) : ((1 << (bit)) >> 8))
#endif

#define __SPRT_ISUPPER __SPRT_ISbit (0)
#define __SPRT_ISLOWER __SPRT_ISbit (1)
#define __SPRT_ISALPHA __SPRT_ISbit (2)
#define __SPRT_ISDIGIT __SPRT_ISbit (3)
#define __SPRT_ISXDIGIT __SPRT_ISbit (4)
#define __SPRT_ISSPACE __SPRT_ISbit (5)
#define __SPRT_ISPRINT __SPRT_ISbit (6)
#define __SPRT_ISGRAPH __SPRT_ISbit (7)
#define __SPRT_ISBLANK __SPRT_ISbit (8)
#define __SPRT_ISCNTRL __SPRT_ISbit (9)
#define __SPRT_ISPUNCT __SPRT_ISbit (10)
#define __SPRT_ISALNUM __SPRT_ISbit (11)
