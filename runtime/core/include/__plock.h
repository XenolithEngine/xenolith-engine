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

#include <sprt/cxx/unordered_map>
#include <sprt/cxx/detail/allocator_local.h>
#include <sprt/runtime/thread/qmutex.h>
#include <sprt/runtime/thread/rmutex.h>

namespace sprt {

template <typename Key, typename Value, typename Hash = sprt::hash<void>,
		typename Pred = sprt::equal_to<void>>
using __plock_unordered_map =
		__unordered_map<Key, Value, Hash, Pred, detail::AllocatorLocal<pair<const Key, Value>>>;

struct __plock_data {
	__rmutex_data data;
	uint32_t refcount = 0;
	uint32_t flags = 0;
};

struct __plock_storage {
	__plock_unordered_map<__sprt_sprt_plock_t, __plock_data *> plocks;
	sprt::qmutex plockMutex;
};

__plock_storage *__libc_get_plock_storage();

} // namespace sprt

#endif // RUNTIME_CORE_INCLUDE___PLOCK_H_
