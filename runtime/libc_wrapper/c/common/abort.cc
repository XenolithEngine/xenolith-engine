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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_assert.h>
#include <sprt/cxx/string>
#include <sprt/runtime/log.h>

#include <stdlib.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(atexit_impl)(void (*cb)(void)) { return ::atexit(cb); }

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(exit_impl)(int ret) { ::exit(ret); }

__SPRT_C_FUNC void __SPRT_ID(_Exit_impl)(int ret) { ::_Exit(ret); }

__SPRT_C_FUNC int __SPRT_ID(at_quick_exit_impl)(void (*cb)(void)) { return ::at_quick_exit(cb); }

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(quick_exit_impl)(int ret) { ::quick_exit(ret); }

} // namespace sprt
