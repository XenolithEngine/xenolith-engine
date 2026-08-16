// Embox setjmp buffer layout.
//
// Embox libc ships a POSIX <setjmp.h> that typedefs jmp_buf / sigjmp_buf to
// __jmp_buf from <asm/setjmp.h>. We cannot #include <setjmp.h> here: the sprt
// umbrella setjmp.h forwards back into this chain. Pull the arch header
// directly so the buffer layout matches sizeof(jmp_buf) in runtime_core_setjmp.cpp.
#include <asm/setjmp.h>

typedef __jmp_buf __SPRT_ID(native_jmp_buf);
typedef __jmp_buf __SPRT_ID(native_sigjmp_buf);
