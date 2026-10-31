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

// Relative include: the Tau kernel and bare modules compile this unit directly,
// without the module's include path.
#include "../include/sprt_fs_config.h"

namespace sprt::fs {

using size_t = __SIZE_TYPE__;
using uintptr_t = __UINTPTR_TYPE__;
using uchar = unsigned char;

} // namespace sprt::fs

// no_builtin keeps the loops from being turned back into calls to these functions,
// whatever flags the including project uses.
#define __SPRT_FS_DEF __SPRT_C_FUNC __attribute__((no_builtin))

#include "sprt_fs_mem.cc"
#include "sprt_fs_str.cc"
