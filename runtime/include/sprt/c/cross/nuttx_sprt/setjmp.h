// NuttX setjmp buffer layout.
//
// NuttX libc defines the buffer as `struct setjmp_buf_s` (jmp_buf) and
// `struct sigsetjmp_buf_s` (sigjmp_buf). These live in arch/setjmp.h and
// nuttx/lib/setjmp.h respectively (NOT in a top-level <setjmp.h> — NuttX libc
// is non-conforming there, see the <setjmp.h> shim this sysroot drops).
//
// We CANNOT just #include <setjmp.h> here: the sprt runtime ships its own
// include_libc/setjmp.h wrapper that forwards back into the sprt chain, which
// would recurse. Pull the NuttX arch/lib headers directly via their relative
// paths so the struct definitions land, then alias under the sprt-mangled
// names the runtime uses to round-trip buffers through its setjmp/longjmp
// machinery.
//
// nuttx/lib/setjmp.h gates sigsetjmp_buf_s behind CONFIG_ARCH_SETJMP_H. The
// exported .config carries it (CONFIG_ARCH_SETJMP_H=y) but as a kconfig "y"
// string, not a defined macro; force-define it here so the struct lands.
#ifndef CONFIG_ARCH_SETJMP_H
#define CONFIG_ARCH_SETJMP_H 1
#endif

// nuttx/lib/setjmp.h carries an inline siglongjmp() that spells NULL, but the
// only thing it includes is <signal.h>. A runtime TU has always pulled <stddef.h>
// in through something else by the time it gets here; a consumer TU whose first
// include is <setjmp.h> has not, and the header then fails to parse. Define the
// one name it is missing instead of dragging all of <stddef.h> in here.
#include <sprt/c/bits/__sprt_null.h>
#ifndef NULL
#define NULL __SPRT_NULL
#endif

#include <arch/setjmp.h>
#include <nuttx/lib/setjmp.h>

typedef struct setjmp_buf_s __SPRT_ID(native_jmp_buf)[1];
typedef struct sigsetjmp_buf_s __SPRT_ID(native_sigjmp_buf)[1];
