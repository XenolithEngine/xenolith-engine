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

#ifndef RUNTIME_CORE_INCLUDE___PLOCK_H_
#define RUNTIME_CORE_INCLUDE___PLOCK_H_

#include <sprt/runtime/thread/qmutex.h>
#include <sprt/runtime/thread/rmutex.h>

namespace sprt {

struct __plock_data {
	__rmutex_data data;
	uint32_t refcount = 0;
	uint32_t flags = 0;
	__sprt_sprt_plock_t lock = nullptr;
	__plock_data *next = nullptr;
};

// The locks in use, chained in fixed buckets by address: taking a lock allocates only its data,
// and a failed allocation is the lock's error.
struct __plock_storage {
	static constexpr uintptr_t Buckets = 64;

	__plock_data *buckets[Buckets] = {};
	sprt::qmutex plockMutex;

	// The link that holds the lock's data, or the empty link at the end of its chain.
	__plock_data **find(__sprt_sprt_plock_t lock) {
		auto link = &buckets[(reinterpret_cast<uintptr_t>(lock) >> 4) % Buckets];
		while (*link && (*link)->lock != lock) { link = &(*link)->next; }
		return link;
	}
};

__plock_storage *__libc_get_plock_storage();

} // namespace sprt

#endif // RUNTIME_CORE_INCLUDE___PLOCK_H_
