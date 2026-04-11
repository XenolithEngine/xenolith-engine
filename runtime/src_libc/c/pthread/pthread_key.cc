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

#include "pthread_native.h"

namespace sprt::_thread {

__SPRT_C_FUNC int __SPRT_ID(
		pthread_key_create)(__SPRT_ID(pthread_key_t) * key, void (*cb)(void *)) {
	if (!key) {
		return EINVAL;
	}

	auto pool = __thread_pool::get();

	*key = pool->nkeys.fetch_add(1);

	unique_lock lock(pool->mutex);
	pool->keys.emplace(*key, __key_data{cb, 1});
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_key_delete)(__SPRT_ID(pthread_key_t) key) {
	auto pool = __thread_pool::get();
	unique_lock lock(pool->mutex);

	auto it = pool->keys.find(key);
	if (it == pool->keys.end()) {
		return EINVAL;
	}

	if (_atomic::fetchSub(&it->second.refcount, uint32_t(1)) == 1) {
		pool->keys.erase(it);
	}
	return 0;
}

__SPRT_C_FUNC void *__SPRT_ID(pthread_getspecific)(__SPRT_ID(pthread_key_t) key) {
	// we dont want to attach this thread
	auto self = thread_t::self_noattach();
	if (!self) {
		return nullptr;
	}

	auto it = self->threadKeyStorage->find(key);
	if (it != self->threadKeyStorage->end()) {
		return const_cast<void *>(it->second.value);
	}

	return nullptr;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_setspecific)(__SPRT_ID(pthread_key_t) key, const void *val) {
	auto self = thread_t::self();

	if (!self) {
		return EINVAL;
	}

	auto it = self->threadKeyStorage->find(key);
	if (it == self->threadKeyStorage->end()) {
		__key_data *data = nullptr;

		auto pool = __thread_pool::get();

		unique_lock lock(pool->mutex);
		auto keyIt = pool->keys.find(key);
		if (keyIt == pool->keys.end()) {
			return EINVAL;
		}

		_atomic::fetchAdd(&keyIt->second.refcount, uint32_t(1));
		data = &keyIt->second;
		lock.unlock();

		self->threadKeyStorage->emplace(key, __key_specific{data, val});
	} else {
		it->second.value = val;
	}
	return 0;
}

} // namespace sprt::_thread
