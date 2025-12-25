#ifndef CORE_RUNTIME_INCLUDE_C_BITS_FCNTL_H_
#define CORE_RUNTIME_INCLUDE_C_BITS_FCNTL_H_

#define __SPRT_O_CREAT        0100
#define __SPRT_O_EXCL         0200
#define __SPRT_O_NOCTTY       0400
#define __SPRT_O_TRUNC       01000
#define __SPRT_O_APPEND      02000
#define __SPRT_O_NONBLOCK    04000
#define __SPRT_O_DSYNC      010000
#define __SPRT_O_SYNC     04010000
#define __SPRT_O_RSYNC    04010000
#define __SPRT_O_DIRECTORY 0200000
#define __SPRT_O_NOFOLLOW  0400000
#define __SPRT_O_CLOEXEC  02000000

#define __SPRT_O_ASYNC      020000
#define __SPRT_O_DIRECT     040000
#define __SPRT_O_LARGEFILE 0100000
#define __SPRT_O_NOATIME  01000000
#define __SPRT_O_PATH    010000000
#define __SPRT_O_TMPFILE 020200000
#define __SPRT_O_NDELAY __SPRT_O_NONBLOCK

#define __SPRT_F_DUPFD  0
#define __SPRT_F_GETFD  1
#define __SPRT_F_SETFD  2
#define __SPRT_F_GETFL  3
#define __SPRT_F_SETFL  4

#define __SPRT_F_SETOWN 8
#define __SPRT_F_GETOWN 9
#define __SPRT_F_SETSIG 10
#define __SPRT_F_GETSIG 11

#ifndef __LP64__
#define __SPRT_F_GETLK 12
#define __SPRT_F_SETLK 13
#define __SPRT_F_SETLKW 14
#else
#define __SPRT_F_GETLK 5
#define __SPRT_F_SETLK 6
#define __SPRT_F_SETLKW 7
#endif

#define __SPRT_F_SETOWN_EX 15
#define __SPRT_F_GETOWN_EX 16

#define __SPRT_F_GETOWNER_UIDS 17

#endif
