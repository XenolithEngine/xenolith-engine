// NuttX does NOT follow the asm-generic O_*/F_* layout that glibc and musl share,
// so this cannot forward to linux_sprt.
//
// This matters beyond the static_asserts in the wrapper: __sprt_open()/__sprt_fcntl()
// pass their flag and command arguments straight to the NuttX libc without any
// translation, and outside __SPRT_BUILD these macros ARE the O_*/F_* the application
// sees (include_libc/fcntl.h). Numbers that disagree with the libc are silently
// wrong, not merely unverified - with the Linux table, __SPRT_F_GETFL (3) reaches
// NuttX as F_GETLEASE and __SPRT_F_SETFL (4) as F_GETLK.
//
// Values from NuttX include/fcntl.h. NuttX deliberately matches Linux for the
// access modes and the low O_ bits, and diverges from bit 14 up; every macro is
// spelled out here rather than partially forwarded so a NuttX bump that shifts a
// bit shows up as an assert failure in SPRuntimeCFcntl.cpp.

// clang-format off
#define __SPRT_O_ACCMODE 03
#define __SPRT_O_RDONLY  00
#define __SPRT_O_WRONLY  01
#define __SPRT_O_RDWR    02

#define __SPRT_O_CREAT        0100  // 1 << 6
#define __SPRT_O_EXCL         0200  // 1 << 7
#define __SPRT_O_NOCTTY       0400  // 1 << 8
#define __SPRT_O_TRUNC       01000  // 1 << 9
#define __SPRT_O_APPEND      02000  // 1 << 10
#define __SPRT_O_NONBLOCK    04000  // 1 << 11
#define __SPRT_O_DSYNC      010000  // 1 << 12
#define __SPRT_O_ASYNC      020000  // 1 << 13

// From here on NuttX and Linux part ways: NuttX packs O_DIRECT/O_LARGEFILE into
// bits 14/15, which Linux/aarch64 leaves for O_DIRECTORY/O_NOFOLLOW.
#define __SPRT_O_DIRECT     040000  // 1 << 14 (Linux: 0200000)
#define __SPRT_O_LARGEFILE 0100000  // 1 << 15 (Linux/64-bit: unused, 0)
#define __SPRT_O_DIRECTORY 0200000  // 1 << 16 (Linux: 040000)
#define __SPRT_O_NOFOLLOW  0400000  // 1 << 17 (Linux: 0100000)

#define __SPRT_O_NOATIME  01000000  // 1 << 18
#define __SPRT_O_CLOEXEC  02000000  // 1 << 19
#define __SPRT_O_PATH    010000000  // 1 << 21

// NuttX composes these the same way Linux does, from a private __O_SYNC (1 << 20)
// and __O_TMPFILE (1 << 22). O_TMPFILE carries O_DIRECTORY, so it moves with it.
#define __SPRT_O_SYNC     04010000  // __O_SYNC | O_DSYNC
#define __SPRT_O_RSYNC    04010000  // == O_SYNC
#define __SPRT_O_TMPFILE 020200000  // __O_TMPFILE | O_DIRECTORY (Linux: 020040000)

#define __SPRT_O_NDELAY __SPRT_O_NONBLOCK

// NuttX has no O_EXEC / O_SEARCH; O_PATH is the closest thing it offers, and the
// wrapper's asserts for them are #ifdef'd on the native spelling, so nothing pins
// this choice.
#define __SPRT_O_SEARCH   __SPRT_O_PATH
#define __SPRT_O_EXEC     __SPRT_O_PATH

#define __SPRT_O_INHERITABLE 0 // non-standard, no-op on this platform

// fcntl() commands. NuttX numbers these alphabetically from 0, sharing only
// F_DUPFD/F_GETFD with Linux.
#define __SPRT_F_DUPFD          0
#define __SPRT_F_GETFD          1
#define __SPRT_F_GETFL          2
#define __SPRT_F_GETLEASE       3
#define __SPRT_F_GETLK          4
#define __SPRT_F_GETOWN         5
#define __SPRT_F_GETSIG         6
#define __SPRT_F_NOTIFY         7
#define __SPRT_F_SETFD          8
#define __SPRT_F_SETFL          9
#define __SPRT_F_SETLEASE      10
#define __SPRT_F_SETLK         11
#define __SPRT_F_SETLKW        12
#define __SPRT_F_SETOWN        13
#define __SPRT_F_SETSIG        14
#define __SPRT_F_GETPATH       15
#define __SPRT_F_ADD_SEALS     16
#define __SPRT_F_GET_SEALS     17
#define __SPRT_F_DUPFD_CLOEXEC 18
#define __SPRT_F_SETPIPE_SZ    19
#define __SPRT_F_GETPIPE_SZ    20

// NuttX implements none of these. They are given values outside the command range
// it accepts, deliberately NOT their Linux numbers: Linux F_SETOWN_EX is 15, which
// is NuttX's F_GETPATH, so reusing the Linux table would turn an unsupported
// request into a different, valid one. As it stands NuttX rejects them with EINVAL.
#define __SPRT_F_SETOWN_EX     0x10000
#define __SPRT_F_GETOWN_EX     0x10001
#define __SPRT_F_GETOWNER_UIDS 0x10002

// __SPRT_F_OFD_* is deliberately left undefined - include_libc/fcntl.h keys the
// whole open-file-description lock family off #ifdef __SPRT_F_OFD_GETLK.

#define __SPRT_F_SEAL_SEAL         0x0001
#define __SPRT_F_SEAL_SHRINK       0x0002
#define __SPRT_F_SEAL_GROW         0x0004
#define __SPRT_F_SEAL_WRITE        0x0008
#define __SPRT_F_SEAL_FUTURE_WRITE 0x0010

#define __SPRT_F_RDLCK 0
#define __SPRT_F_WRLCK 1
#define __SPRT_F_UNLCK 2

#define __SPRT_FD_CLOEXEC 1

// The *at() flags do match Linux.
#define __SPRT_AT_FDCWD - 100
#define __SPRT_AT_SYMLINK_NOFOLLOW 0x100
#define __SPRT_AT_SYMLINK_FOLLOW 0x400
#define __SPRT_AT_NO_AUTOMOUNT 0x800
#define __SPRT_AT_EMPTY_PATH 0x1000
#define __SPRT_AT_EACCESS 0x200
#define __SPRT_AT_REMOVEDIR 0x200

// statx(2) is Linux-only and NuttX declares none of these, so nothing can pin
// them; they are kept at their Linux values because sprt/wrappers/unistd/unistd.h
// re-exports them unconditionally. __SPRT_AT_HANDLE_* is left undefined instead -
// that umbrella keys the name_to_handle_at flags off #ifdef.
#define __SPRT_AT_STATX_SYNC_TYPE 0x6000
#define __SPRT_AT_STATX_SYNC_AS_STAT 0x0000
#define __SPRT_AT_STATX_FORCE_SYNC 0x2000
#define __SPRT_AT_STATX_DONT_SYNC 0x4000
#define __SPRT_AT_RECURSIVE 0x8000
// clang-format on
