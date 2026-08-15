// NuttX does NOT share the Linux/glibc dirent layout, so this is one of the few
// cross headers that cannot forward to linux_sprt.
//
// NuttX (include/dirent.h):
//     struct dirent { ino_t d_ino; uint8_t d_type; char d_name[NAME_MAX + 1]; };
// i.e. no d_off / d_reclen, a 32-bit ino_t (sys/types.h) and a name buffer sized
// by the kconfig, not a fixed 256.
//
// The layout must match byte for byte, not merely be "large enough": readdir()
// returns a pointer into the libc's own DIR::entry, which the wrapper hands back
// as a `struct __SPRT_DIRENT_NAME *`, and outside __SPRT_BUILD this struct IS the
// `struct dirent` application code sees (see include_libc/dirent.h). A wider sprt
// shape would put d_type and d_name at the wrong offsets and read garbage.
//
// The DT_* values in __sprt_dirent.h already agree with NuttX's.

#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_uint8_t.h>

// NAME_MAX is CONFIG_NAME_MAX (via _POSIX_NAME_MAX in NuttX's <limits.h>), so the
// size of d_name is a kconfig value. Read it from the generated config header
// rather than <limits.h>: <limits.h> silently falls back to 32 when the config is
// not visible, which would produce a struct that compiles but does not match the
// libc that was built with the real value. nuttx/config.h is macro-only.
#include <nuttx/config.h>

#ifndef CONFIG_NAME_MAX
#error "NuttX CONFIG_NAME_MAX is not visible - struct dirent would get the wrong size"
#endif

// Opaque: NuttX's DIR is a complete `struct { int fd; struct dirent entry; }`, but
// sprt only ever passes it around by pointer, so the incomplete type is enough and
// keeps the handle from being copied by value.
typedef struct __SPRT_DIR __SPRT_ID(DIR);

struct __SPRT_DIRENT_NAME {
	__SPRT_ID(uint32_t) d_ino;
	__SPRT_ID(uint8_t) d_type;
	char d_name[CONFIG_NAME_MAX + 1];
};
