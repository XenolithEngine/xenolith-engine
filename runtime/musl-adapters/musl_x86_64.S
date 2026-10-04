/* musl's hand-written x86_64 assembly, for the freestanding x86_64 targets.
 *
 * musl_string.c skips memcpy, memmove and memset on x86_64 and musl_fenv.c leaves
 * the fenv primitives to the platform layer; hosted targets take them from the
 * platform libc. Conditioned on predefines only, as musl_aarch64.S.
 */

#if defined(__x86_64__) && defined(__TAU__)

#include "../musl-libc/src/string/x86_64/memcpy.s"
#include "../musl-libc/src/string/x86_64/memmove.s"
#include "../musl-libc/src/string/x86_64/memset.s"

#include "../musl-libc/src/fenv/x86_64/fenv.s"

#include "../musl-libc/src/setjmp/x86_64/setjmp.s"
#include "../musl-libc/src/setjmp/x86_64/longjmp.s"

#endif
