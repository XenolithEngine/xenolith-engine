// Embox does NOT follow the asm-generic O_*/F_* layout that glibc and musl share,
// so this cannot forward to linux_sprt.
//
// This matters beyond the static_asserts in the wrapper: __sprt_open()/__sprt_fcntl()
// pass their flag and command arguments straight to the Embox libc without any
// translation, and outside __SPRT_BUILD these macros ARE the O_*/F_* the application
// sees (include_libc/fcntl.h). Numbers that disagree with the libc are silently
// wrong, not merely unverified.
//
// Values from Embox src/compat/posix/include/fcntl.h. Every macro is spelled out
// here rather than partially forwarded so an Embox bump that shifts a bit shows up
// as an assert failure in SPRuntimeCFcntl.cpp.

// clang-format off
#define __SPRT_O_ACCMODE 0x0003
#define __SPRT_O_RDONLY  0x0000
#define __SPRT_O_WRONLY  0x0001
#define __SPRT_O_RDWR    0x0002

#define __SPRT_O_APPEND  0x0008
#define __SPRT_O_CLOEXEC 0x0010  // Embox spells it FD_CLOEXEC and aliases O_CLOEXEC to it
#define __SPRT_O_CREAT   0x0100
#define __SPRT_O_TRUNC   0x0200
#define __SPRT_O_EXCL    0x0400
#define __SPRT_O_DIRECT  0x0800
#define __SPRT_O_NONBLOCK 0x1000
#define __SPRT_O_NDELAY  __SPRT_O_NONBLOCK
#define __SPRT_O_DIRECTORY 0x2000
#define __SPRT_O_SEARCH  0x4000
#define __SPRT_O_PATH    __SPRT_O_SEARCH  // Embox: O_PATH is an alias of O_SEARCH
#define __SPRT_O_NOFOLLOW 0x8000
#define __SPRT_O_NOCTTY  0x10000
#define __SPRT_O_SYNC    0x04000000

// Embox has no O_EXEC; O_SEARCH is the closest thing it offers, the way nuttx_sprt
// answers with O_PATH. The wrapper's assert for it is #ifdef'd on the native
// spelling, so nothing pins this choice.
#define __SPRT_O_EXEC    __SPRT_O_SEARCH

// Embox has a single sync flag, and include_libc/fcntl.h re-exports O_DSYNC
// unconditionally: answer with full sync, which is stricter than data-only sync,
// never weaker. __SPRT_O_RSYNC stays undefined - that name is #ifdef-keyed, so
// Embox simply does not offer it.
#define __SPRT_O_DSYNC   __SPRT_O_SYNC

// No counterpart in Embox, but include_libc/fcntl.h re-exports O_ASYNC
// unconditionally. Bit 17 is outside every flag Embox tests, so open() ignores it
// instead of turning into some other request.
#define __SPRT_O_ASYNC   0x00020000

#define __SPRT_O_INHERITABLE 0 // non-standard, no-op on this platform

// __SPRT_O_LARGEFILE / __SPRT_O_NOATIME / __SPRT_O_TMPFILE are deliberately left
// undefined: Embox has none of them, and include_libc/fcntl.h keys each of those
// names (and the O_TMPFILE branch of its inline open()) off #ifdef.

// fcntl() commands, in Embox's own numbering - it shares only F_GETFD/F_SETFD
// with Linux.
#define __SPRT_F_GETFD          0
#define __SPRT_F_SETFD          1
#define __SPRT_F_GETPIPE_SZ     2
#define __SPRT_F_SETPIPE_SZ     3
#define __SPRT_F_GETLK          4
#define __SPRT_F_SETLK          5
#define __SPRT_F_SETLKW         6
#define __SPRT_F_RDLCK          7
#define __SPRT_F_UNLCK          8
#define __SPRT_F_WRLCK          9
#define __SPRT_F_GETFL         10
#define __SPRT_F_SETFL         11
#define __SPRT_F_DUPFD         12
#define __SPRT_F_DUPFD_CLOEXEC 13

// Embox implements none of these. They are given values outside the command range
// it accepts - NOT their Linux numbers, which would land on a different, valid
// Embox command (Linux F_GETFL is 3, which is Embox's F_SETPIPE_SZ). As it stands
// Embox rejects them instead of silently doing something else.
#define __SPRT_F_SETOWN           0x10000
#define __SPRT_F_GETOWN           0x10001
#define __SPRT_F_SETSIG           0x10002
#define __SPRT_F_GETSIG           0x10003
#define __SPRT_F_SETOWN_EX        0x10004
#define __SPRT_F_GETOWN_EX        0x10005
#define __SPRT_F_GETOWNER_UIDS    0x10006
#define __SPRT_F_SETLEASE         0x10007
#define __SPRT_F_GETLEASE         0x10008
#define __SPRT_F_NOTIFY           0x10009
#define __SPRT_F_ADD_SEALS        0x1000a
#define __SPRT_F_GET_SEALS        0x1000b
#define __SPRT_F_GET_RW_HINT      0x1000c
#define __SPRT_F_SET_RW_HINT      0x1000d
#define __SPRT_F_GET_FILE_RW_HINT 0x1000e
#define __SPRT_F_SET_FILE_RW_HINT 0x1000f

// __SPRT_F_OFD_* is deliberately left undefined - include_libc/fcntl.h keys the
// whole open-file-description lock family off #ifdef __SPRT_F_OFD_GETLK.

#define __SPRT_F_SEAL_SEAL         0x0001
#define __SPRT_F_SEAL_SHRINK       0x0002
#define __SPRT_F_SEAL_GROW         0x0004
#define __SPRT_F_SEAL_WRITE        0x0008
#define __SPRT_F_SEAL_FUTURE_WRITE 0x0010

#define __SPRT_FD_CLOEXEC 0x0010

#define __SPRT_AT_FDCWD -100

// Embox declares no AT_* beyond AT_FDCWD, so nothing can pin these; they are kept
// at their Linux values because sprt/wrappers/unistd/unistd.h re-exports them
// unconditionally. __SPRT_AT_HANDLE_* is left undefined instead - that umbrella
// keys the name_to_handle_at flags off #ifdef.
#define __SPRT_AT_SYMLINK_NOFOLLOW 0x100
#define __SPRT_AT_SYMLINK_FOLLOW 0x400
#define __SPRT_AT_NO_AUTOMOUNT 0x800
#define __SPRT_AT_EMPTY_PATH 0x1000
#define __SPRT_AT_EACCESS 0x200
#define __SPRT_AT_REMOVEDIR 0x200
#define __SPRT_AT_STATX_SYNC_TYPE 0x6000
#define __SPRT_AT_STATX_SYNC_AS_STAT 0x0000
#define __SPRT_AT_STATX_FORCE_SYNC 0x2000
#define __SPRT_AT_STATX_DONT_SYNC 0x4000
#define __SPRT_AT_RECURSIVE 0x8000
// clang-format on
