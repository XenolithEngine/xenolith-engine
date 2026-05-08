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

#ifndef RUNTIME_CORE_PTHREAD_PTHREAD_STRUCT_H_
#define RUNTIME_CORE_PTHREAD_PTHREAD_STRUCT_H_

#include <sprt/cxx/detail/ctypes.h>
#include <sprt/runtime/mem/pool.h>
#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_stdio.h>

namespace sprt::_thread {

struct thread_base_t {
	thread_base_t *next = nullptr;
	void *handle = nullptr;
	__sprt_pid_t threadId = 0;

	void *(*cb)(void *) = nullptr;
	void *arg = nullptr;

	uintptr_t lowStack = 0;
	uintptr_t highStack = 0;

	// Memory pool to use with thread-scoped memory,
	// Should be acquired from memory::get_thread_support_pool() in thread's context
	memory::pool_t *threadMemPool = nullptr;

	__SPRT_ID(FILE) *ioLocks = nullptr;
};

} // namespace sprt::_thread

#endif // RUNTIME_CORE_PTHREAD_PTHREAD_STRUCT_H_
