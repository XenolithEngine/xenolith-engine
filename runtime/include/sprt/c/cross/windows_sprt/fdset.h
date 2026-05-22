#ifndef __SPRT_FD_SETSIZE
#define __SPRT_FD_SETSIZE      64
#endif /* FD_SETSIZE */

typedef __SPRT_ID(uintptr_t) SOCKET;

typedef struct fd_set {
	unsigned int fd_count; /* how many are SET? */
	SOCKET fd_array[__SPRT_FD_SETSIZE]; /* an array of SOCKETs */
} __SPRT_ID(fd_set);

#ifdef __cplusplus
extern "C" {
#endif

extern __SPRT_DLLIMPORT int __WSAFDIsSet(SOCKET, __SPRT_ID(fd_set) *);

#ifdef __cplusplus
}
#endif

#define __SPRT_FD_CLR(fd, set) do { \
    unsigned int __i; \
    for (__i = 0; __i < ((__SPRT_ID(fd_set)  *)(set))->fd_count ; __i++) { \
        if (((__SPRT_ID(fd_set)  *)(set))->fd_array[__i] == fd) { \
            while (__i < ((__SPRT_ID(fd_set)  *)(set))->fd_count-1) { \
                ((__SPRT_ID(fd_set)  *)(set))->fd_array[__i] = \
                    ((__SPRT_ID(fd_set)  *)(set))->fd_array[__i+1]; \
                __i++; \
            } \
            ((__SPRT_ID(fd_set)  *)(set))->fd_count--; \
            break; \
        } \
    } \
} while(0)

#define __SPRT_FD_SET(fd, set) do { \
    if (((__SPRT_ID(fd_set)  *)(set))->fd_count < __SPRT_FD_SETSIZE) \
        ((__SPRT_ID(fd_set)  *)(set))->fd_array[((__SPRT_ID(fd_set)  *)(set))->fd_count++]=(fd);\
} while(0)

#define __SPRT_FD_ZERO(set) (((__SPRT_ID(fd_set)  *)(set))->fd_count=0)

#define __SPRT_FD_ISSET(fd, set) __WSAFDIsSet((SOCKET)(fd), (__SPRT_ID(fd_set)  *)(set))
