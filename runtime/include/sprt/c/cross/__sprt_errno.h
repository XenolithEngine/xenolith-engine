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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_ERRNO_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_ERRNO_H_

#include <sprt/c/bits/__sprt_def.h>

// Select the per-platform errno list. errno numbering is OS-specific (not arch-specific), so
// only one platform-level header is pulled. Unlike cross/__sprt_signal.h this does NOT use the
// SPRT_CROSS_CONFIG_NAME() stringizing helper: that macro-expands its argument, and once a
// system <errno.h> is in scope `errno` is itself a macro (e.g. (*__error()) on macOS), which
// would corrupt the `.../errno.h` path. A literal #include <...> is never macro-expanded.
#if SPRT_IOS
#include <sprt/c/cross/ios_sprt/errno.h>
#elif SPRT_MACOS
#include <sprt/c/cross/macos_sprt/errno.h>
#elif SPRT_ANDROID
#include <sprt/c/cross/android_sprt/errno.h>
#elif SPRT_LINUX
#include <sprt/c/cross/linux_sprt/errno.h>
#elif SPRT_WINDOWS
#include <sprt/c/cross/windows_sprt/errno.h>
#else
#include <sprt/c/cross/linux_sprt/errno.h>
#endif

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_ERRNO_H_
