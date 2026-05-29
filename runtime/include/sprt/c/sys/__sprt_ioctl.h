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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_IOCTL_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_IOCTL_H_

#include <sprt/c/bits/__sprt_intptr_t.h>

#define __SPRT_TIOCGWINSZ	0x5413
#define __SPRT_TIOCSWINSZ	0x5414

__SPRT_BEGIN_DECL

SPRT_API
int __SPRT_ID(ioctl)(int, int, ...) __SPRT_NOEXCEPT;

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_IOCTL_H_
