// Embox does NOT share the Linux/glibc dirent layout, so this is one of the few
// cross headers that cannot forward to linux_sprt.
//
// Embox (src/compat/posix/include/dirent.h):
//     struct dirent { ino_t d_ino; char d_name[NAME_MAX]; off_t d_off;
//                     unsigned short d_reclen; unsigned char d_type; };
// i.e. d_name sits SECOND, right after d_ino, and the Linux-compatibility fields
// trail it; d_name is NAME_MAX bytes with no room for a terminator beyond it.
//
// The layout must match byte for byte, not merely be "large enough": readdir()
// returns a pointer into the libc's own DIR::entry, which the wrapper hands back
// as a `struct __SPRT_DIRENT_NAME *`, and outside __SPRT_BUILD this struct IS the
// `struct dirent` application code sees (see include_libc/dirent.h). The Linux
// shape would put d_name 8 bytes past where Embox writes it.

#include <sprt/c/bits/__sprt_uint16_t.h>
#include <sprt/c/bits/__sprt_uint8_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>
#include <sprt/c/bits/__sprt_int64_t.h>

// NAME_MAX is an Embox module option (embox.compat.libc.limits: name_max), so the
// size of d_name is a config value - read it from <limits.h> rather than hardcode
// it, the way nuttx_sprt reads CONFIG_NAME_MAX. <limits.h> resolves it through the
// generated config header; without that it silently falls back to 32, which would
// produce a struct that compiles but does not match the libc that was built.
#include <limits.h>

#ifndef NAME_MAX
#error "Embox NAME_MAX is not visible - struct dirent would get the wrong size"
#endif

// Opaque: Embox's DIR is a complete struct, but sprt only ever passes it around by
// pointer, so the incomplete type is enough and keeps the handle from being copied
// by value.
typedef struct __SPRT_DIR __SPRT_ID(DIR);

struct __SPRT_DIRENT_NAME {
	__SPRT_ID(uint64_t) d_ino; // Embox: unsigned long
	char d_name[NAME_MAX];
	__SPRT_ID(int64_t) d_off; // Embox: long
	__SPRT_ID(uint16_t) d_reclen;
	__SPRT_ID(uint8_t) d_type;
};

// Embox numbers the d_type codes itself: it starts at DT_BLK and steps by two in
// alphabetical order, where Linux/BSD interleave. __sprt_dirent.h #ifndef-guards
// the shared table, so the values below win. SPRuntimeCDirent.cpp pins them.
// clang-format off
#define __SPRT_DT_UNKNOWN 0
#define __SPRT_DT_BLK     2  // Linux: 6
#define __SPRT_DT_CHR     4  // Linux: 2
#define __SPRT_DT_DIR     6  // Linux: 4
#define __SPRT_DT_FIFO    8  // Linux: 1
#define __SPRT_DT_LNK     10
#define __SPRT_DT_REG     12 // Linux: 8
#define __SPRT_DT_SOCK    14 // Linux: 12
// Embox has no whiteout entries; 16 is outside the codes it produces.
#define __SPRT_DT_WHT     16 // Linux: 14
// clang-format on
