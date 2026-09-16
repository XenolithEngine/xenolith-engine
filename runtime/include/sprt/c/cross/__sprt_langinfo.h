/**
Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_LANGINFO_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_LANGINFO_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/cross/__sprt_config.h>

// Per-platform nl_item base values (the langinfo item numbering is ABI and
// differs across libcs: glibc encodes the category in the high bits, Bionic and
// Darwin use plain sequential ids). The libc_wrapper bridge static-asserts these
// against the platform <langinfo.h>.
// clang-format off
#include SPRT_CROSS_CONFIG_NAME(sprt/c/cross/__SPRT_PLATFORM_NAME/langinfo.h)
// clang-format on

#endif
