/* musl's hand-written aarch64 assembly, for the freestanding aarch64 targets.
 *
 * The adapter SCUs above skip several C implementations on x86_64 and aarch64
 * because musl ships assembly for them -- but nothing in this tree ever compiled
 * that assembly. Every other aarch64 target is hosted (platform libc) or Windows
 * (MSVC CRT plus libc_impl/asm/Windows/aarch64), so the symbols always came from
 * somewhere else. Embox EL0 is the first freestanding aarch64 target, and it is
 * where the gap shows up as undefined symbols at link time:
 *
 *   memcpy/memset      musl_string.c skips the C versions here
 *   fenv primitives    musl_fenv.c defers them to "the platform layer"
 *   setjmp/longjmp     libc_impl has no aarch64 asm directory
 *
 * memmove and siglongjmp have no aarch64 assembly upstream; the C versions come
 * in through musl_string.c and musl_siglongjmp.c.
 *
 * Conditioned on predefines only -- no #include of the sprt platform header.
 * Assembly is compiled with its own flag set (TARGET_GENERAL_SFLAGS), which
 * carries the target define but not the C include paths, so a header lookup here
 * would fail. The relative includes below resolve against this file.
 */

#if defined(__aarch64__) && defined(__EMBOX_USER__)

#include "../musl-libc/src/string/aarch64/memcpy.S"
#include "../musl-libc/src/string/aarch64/memset.S"

#include "../musl-libc/src/fenv/aarch64/fenv.s"

#include "../musl-libc/src/setjmp/aarch64/setjmp.s"
#include "../musl-libc/src/setjmp/aarch64/longjmp.s"

#endif
