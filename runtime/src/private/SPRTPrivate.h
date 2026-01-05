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

#ifndef CORE_RUNTIME_PRIVATE_SPRTPRIVATE_H_
#define CORE_RUNTIME_PRIVATE_SPRTPRIVATE_H_

#include <sprt/runtime/mem/pool.h>
#include <sprt/runtime/int.h>
#include <sprt/runtime/platform.h>

#if SPRT_ANDROID
#include <time.h>
#endif

namespace sprt::platform {

SPRT_LOCAL bool initialize(AppConfig &&cfg, int &);
SPRT_LOCAL void terminate();

SPRT_LOCAL memory::pool_t *getConfigPool();

} // namespace sprt::platform

namespace sprt::backtrace {

SPRT_LOCAL void initialize();
SPRT_LOCAL void terminate();

} // namespace sprt::backtrace

namespace sprt::filesystem {

SPRT_LOCAL void initialize();
SPRT_LOCAL void terminate();

} // namespace sprt::filesystem

#endif // CORE_RUNTIME_PRIVATE_SPRTPRIVATE_H_
