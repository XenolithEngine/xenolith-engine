
// Embox EL0 exception-runtime bring-up hooks.
//
// Nothing to install. The Windows backend captures ntdll unwind entry points
// here because SEH needs them; on an ELF target the unwinder finds its data
// through .eh_frame_hdr / PT_GNU_EH_FRAME, which the linker script emits and the
// loader maps -- no runtime registration step exists.
//
// That is a claim about the LINK, not about this file, and it is the open half:
// libunwind must be built for this target WITHOUT LIBUNWIND_IS_BAREMETAL (phase
// L4), and the application's linker script must emit PT_GNU_EH_FRAME (B1). If
// either is missing, throw/catch fails at run time and not here.

#include "../../include/__impl_libc.h"

namespace sprt {

bool __init_exceptions(void) { return true; }

void __cleanup_exceptions(void) { }

} // namespace sprt
