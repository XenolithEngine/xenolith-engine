/* musl's hand-written x86_64 assembly, for the freestanding x86_64 targets: setjmp
 * and the x87 long double functions. The string functions and the fenv primitives
 * come from runtime/libc_freestanding. Conditioned on predefines only, as
 * musl_aarch64.S.
 */

#if defined(__x86_64__) && defined(__TAU__)

#include "../musl-libc/src/setjmp/x86_64/setjmp.s"
#include "../musl-libc/src/setjmp/x86_64/longjmp.s"

/* The x87 long double functions, which musl has only here on x86_64; the runtime's wrappers name
 * them, and libsprt.so keeps every wrapper (RT-A-SharedRuntime-1). */
.include "acosl.s"
.include "asinl.s"
.include "atan2l.s"
.include "atanl.s"
.include "exp2l.s"
.include "expl.s"
.include "expm1l.s"
.include "floorl.s"
.include "log10l.s"
.include "log1pl.s"
.include "log2l.s"
.include "logl.s"

#endif
