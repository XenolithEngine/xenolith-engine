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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_USERDATA_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_USERDATA_H_

#include <sprt/runtime/mem/context.h>
#include <sprt/runtime/mem/function.h>
#include <sprt/runtime/stringview.h>

namespace sprt::memory::pool {

SPRT_API void store(pool_t *, void *ptr, const StringView &key,
		memory::function<void()> && = nullptr);

template <typename T = void>
inline T *get(pool_t *pool, const StringView &key) {
	struct Handle : AllocPool {
		void *pointer;
		memory::function<void()> callback;
	};

	void *ptr = nullptr;
	if (pool::userdata_get(&ptr, key.data(), key.size(), pool) == Status::Ok) {
		if (ptr) {
			return (T *)((Handle *)ptr)->pointer;
		}
	}
	return nullptr;
}

inline void store(void *ptr, const StringView &key, memory::function<void()> &&cb = nullptr) {
	store(acquire(), ptr, key, move(cb));
}

template <typename T = void>
inline T *get(const StringView &key) {
	return get<T>(acquire(), key);
}

} // namespace sprt::memory::pool

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_USERDATA_H_
