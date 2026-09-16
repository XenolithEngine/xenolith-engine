/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_ERRNO_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_ERRNO_H_

// errno numbering is platform-specific (Linux/Android, macOS/Darwin, and Windows each differ),
// so the lists live per-platform in cross/<platform>/errno.h and are selected here. Each list
// defines the internal __SPRT_Exxx values — consumed by the runtime and verified against the
// system <errno.h> by the static_asserts in libc_wrapper/c/common/errno.cc — plus the public
// bare Exxx names, the latter guarded by #ifndef EPERM so a hosted build's system <errno.h>
// takes precedence.
#include <sprt/c/cross/__sprt_errno.h>

#endif
