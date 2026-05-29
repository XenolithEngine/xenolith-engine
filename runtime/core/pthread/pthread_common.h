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

#ifndef RUNTIME_CORE_PTHREAD_COMMON_H_
#define RUNTIME_CORE_PTHREAD_COMMON_H_

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/cxx/detail/ctypes.h>

typedef __SPRT_ID(intptr_t) intptr_t;
typedef __SPRT_ID(uintptr_t) uintptr_t;
typedef __SPRT_ID(uint64_t) uint64_t;

#include <unwind.h>

#include "pthread_struct.h"

#if SPRT_WINDOWS
#define SPRT_RUNTHREAD_CALLCONV __stdcall
#else
#define SPRT_RUNTHREAD_CALLCONV
#endif

namespace sprt::_thread {

// platform-specific unwinding information
struct __UnwindInfo {
	_Unwind_Exception excpt;
};

static constexpr int DESTRUCTOR_ITERATIONS = 4;
static constexpr int COMMON_ALIGNMENT = 8;

struct mutex_t;
struct rwlock_t;

// in nanoseconds
using timeout_t = __sprt_sprt_timeout_t;

using key_t = uint32_t;

#if SPRT_WINDOWS
using thread_result_t = unsigned long;
#else
using thread_result_t = void *;
#endif

static constexpr timeout_t Infinite = __SPRT_SPRT_TIMEOUT_INFINITE;

struct __key_data {
	void (*destructor)(void *);
	uint32_t refcount = 1;
};

struct __key_specific {
	__key_data *data = nullptr;
	const void *value = nullptr;
};

} // namespace sprt::_thread

#endif // RUNTIME_CORE_PTHREAD_COMMON_H_
