// clang-format off
// Embox fcntl/open numbers from src/compat/posix/include/fcntl.h (not Linux
// asm-generic). The libc_wrapper passes __SPRT_O_* / __SPRT_F_* through to
// native open/fcntl, so these must match the kernel.

#define __SPRT_O_RDONLY  0x0000
#define __SPRT_O_WRONLY  0x0001
#define __SPRT_O_RDWR    0x0002
#define __SPRT_O_APPEND  0x0008
#define __SPRT_O_CREAT   0x0100
#define __SPRT_O_TRUNC   0x0200
#define __SPRT_O_EXCL    0x0400
#define __SPRT_O_DIRECT  0x0800
#define __SPRT_O_NONBLOCK 0x1000
#define __SPRT_O_NDELAY  __SPRT_O_NONBLOCK
#define __SPRT_O_DIRECTORY 0x2000
#define __SPRT_O_SEARCH  0x4000
#define __SPRT_O_PATH    __SPRT_O_SEARCH
#define __SPRT_O_EXEC    __SPRT_O_PATH
#define __SPRT_O_NOFOLLOW 0x8000
#define __SPRT_O_NOCTTY  0x10000
#define __SPRT_O_SYNC    0x04000000
#define __SPRT_O_DSYNC   __SPRT_O_SYNC
#define __SPRT_O_RSYNC   __SPRT_O_SYNC
#define __SPRT_O_CLOEXEC 0x0010
#define __SPRT_O_INHERITABLE 0
#define __SPRT_O_ASYNC   0
#define __SPRT_O_LARGEFILE 0
#define __SPRT_O_NOATIME 0
#define __SPRT_O_TMPFILE 0
#define __SPRT_O_ACCMODE 0x0003

#define __SPRT_F_GETFD  0
#define __SPRT_F_SETFD  1
#define __SPRT_F_GETPIPE_SZ 2
#define __SPRT_F_SETPIPE_SZ 3
#define __SPRT_F_GETLK  4
#define __SPRT_F_SETLK  5
#define __SPRT_F_SETLKW 6
#define __SPRT_F_RDLCK  7
#define __SPRT_F_UNLCK  8
#define __SPRT_F_WRLCK  9
#define __SPRT_F_GETFL  10
#define __SPRT_F_SETFL  11
#define __SPRT_F_DUPFD  12
#define __SPRT_F_DUPFD_CLOEXEC 13

#define __SPRT_F_SETOWN 0
#define __SPRT_F_GETOWN 0
#define __SPRT_F_SETSIG 0
#define __SPRT_F_GETSIG 0
#define __SPRT_F_SETOWN_EX 0
#define __SPRT_F_GETOWN_EX 0
#define __SPRT_F_GETOWNER_UIDS 0
#define __SPRT_F_OFD_GETLK 0
#define __SPRT_F_OFD_SETLK 0
#define __SPRT_F_OFD_SETLKW 0
#define __SPRT_F_SETLEASE 0
#define __SPRT_F_GETLEASE 0
#define __SPRT_F_NOTIFY 0
#define __SPRT_F_ADD_SEALS 0
#define __SPRT_F_GET_SEALS 0
#define __SPRT_F_GET_RW_HINT 0
#define __SPRT_F_SET_RW_HINT 0
#define __SPRT_F_GET_FILE_RW_HINT 0
#define __SPRT_F_SET_FILE_RW_HINT 0
#define __SPRT_F_SEAL_SEAL 0x0001
#define __SPRT_F_SEAL_SHRINK 0x0002
#define __SPRT_F_SEAL_GROW 0x0004
#define __SPRT_F_SEAL_WRITE 0x0008
#define __SPRT_F_SEAL_FUTURE_WRITE 0x0010

#define __SPRT_FD_CLOEXEC 0x0010

#define __SPRT_AT_FDCWD -100
#define __SPRT_AT_SYMLINK_NOFOLLOW 0x100
#define __SPRT_AT_SYMLINK_FOLLOW 0x400
#define __SPRT_AT_NO_AUTOMOUNT 0x800
#define __SPRT_AT_EMPTY_PATH 0x1000
#define __SPRT_AT_STATX_SYNC_TYPE 0x6000
#define __SPRT_AT_STATX_SYNC_AS_STAT 0x0000
#define __SPRT_AT_STATX_FORCE_SYNC 0x2000
#define __SPRT_AT_STATX_DONT_SYNC 0x4000
#define __SPRT_AT_RECURSIVE 0x8000
#define __SPRT_AT_EACCESS 0x200
#define __SPRT_AT_REMOVEDIR 0x200
#define __SPRT_AT_HANDLE_FID 0x200
#define __SPRT_AT_HANDLE_MNT_ID_UNIQUE 0x001

// clang-format on
