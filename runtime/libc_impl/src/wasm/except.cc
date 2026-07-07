/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

// WebAssembly exception-runtime bring-up hooks.
//
// The Windows build installs a vectored exception handler and captures ntdll
// unwind entry points here (windows/except.cc). wasm has no such userspace
// exception machinery in this milestone (the runtime is built -fno-exceptions;
// real C++ EH / setjmp needs the -fwasm-exceptions lowering, a later milestone),
// so __libc's ctor/dtor just call these no-op hooks.

#include "../../include/__impl_libc.h"

namespace sprt {

bool __init_exceptions(void) { return true; }

void __cleanup_exceptions(void) { }

} // namespace sprt

// libbacktrace is unavailable on wasm (no DWARF backtrace state); return a null state so
// the runtime's SPRuntimeBacktrace degrades to "no symbols" rather than failing to link.
extern "C" void *backtrace_create_state(const char *, int, void *, void *) { return nullptr; }
