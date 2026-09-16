// NuttX packs the MAP_* flags into consecutive bits from 0 instead of following
// the asm-generic layout, and it orders MADV_RANDOM/MADV_SEQUENTIAL the POSIX way
// (POSIX_MADV_SEQUENTIAL == 1) rather than Linux's. mmap()/msync()/madvise() are
// forwarded with their flag arguments untouched, and outside __SPRT_BUILD these
// macros ARE the application's MAP_*/MS_*/MADV_*, so the numbers must be NuttX's.
//
// This header is included before the shared table in sys/__sprt_mman.h, whose
// divergent entries are #ifndef-guarded; the values below win.

// clang-format off

// Renumbered by NuttX (Linux value in the comment).
#define __SPRT_MAP_TYPE       (3 << 0)  // 0x0f
#define __SPRT_MAP_FIXED      (1 << 2)  // 0x10
#define __SPRT_MAP_FILE       (1 << 3)  // 0
#define __SPRT_MAP_ANONYMOUS  (1 << 4)  // 0x20
#define __SPRT_MAP_ANON       __SPRT_MAP_ANONYMOUS
#define __SPRT_MAP_GROWSDOWN  (1 << 5)  // 0x0100
#define __SPRT_MAP_DENYWRITE  (1 << 6)  // 0x0800
#define __SPRT_MAP_EXECUTABLE (1 << 7)  // 0x1000
#define __SPRT_MAP_LOCKED     (1 << 8)  // 0x2000
#define __SPRT_MAP_NORESERVE  (1 << 9)  // 0x4000
#define __SPRT_MAP_POPULATE   (1 << 10) // 0x8000
#define __SPRT_MAP_NONBLOCK   (1 << 11) // 0x10000

#define __SPRT_MS_ASYNC       0x01
#define __SPRT_MS_SYNC        0x02 // Linux: 4
#define __SPRT_MS_INVALIDATE  0x04 // Linux: 2

// NuttX defines MADV_* as aliases of the POSIX_MADV_* codes, which put SEQUENTIAL
// before RANDOM - the reverse of Linux.
#define __SPRT_POSIX_MADV_SEQUENTIAL 1
#define __SPRT_POSIX_MADV_RANDOM     2
#define __SPRT_MADV_SEQUENTIAL       1
#define __SPRT_MADV_RANDOM           2

// __SPRT_MADV_FREE is deliberately left undefined - NuttX has no MADV_FREE, and
// both include_libc/sys/mman.h and the wrapper's assert key off #ifdef.
// clang-format on
