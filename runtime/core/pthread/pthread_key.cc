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

#include "pthread_impl.h"

namespace sprt::_thread {

__SPRT_C_FUNC int __SPRT_ID(
		pthread_key_create)(__SPRT_ID(pthread_key_t) * key, void (*cb)(void *)) {
	if (!key) {
		return EINVAL;
	}

	auto pool = __thread_pool::get();

	unique_lock lock(pool->mutex);

	// POSIX: report EAGAIN once the per-process key limit is reached.
	if (pool->keys.size() >= __SPRT_PTHREAD_KEYS_MAX) {
		return EAGAIN;
	}

	__key_data *data = nullptr;
	for (auto &slot : pool->keySlots) {
		if (_atomic::loadSeq(&slot.refcount) == 0) {
			data = &slot;
			break;
		}
	}
	if (!data) {
		auto mem = __sprt_local_alloc(sizeof(__key_data));
		if (!mem) {
			return EAGAIN;
		}
		data = new (mem, nothrow) __key_data;
	}
	data->destructor = cb;
	_atomic::storeSeq(&data->refcount, uint32_t(1));

	// Publish *key only after the table entry is committed, so a caller never sees
	// a key index with no backing entry.
	auto k = pool->nkeys.fetch_add(1);
	pool->keys.emplace(k, data);
	*key = k;
	return 0;
}

static void __releaseKey(__key_data *data) {
	auto pool = __thread_pool::get();
	if (_atomic::fetchSub(&data->refcount, uint32_t(1)) == 1
			&& (data < pool->keySlots || data >= pool->keySlots + __thread_pool::KeySlots)) {
		destroy_at(data);
		__sprt_local_free(data, sizeof(__key_data));
	}
}

__SPRT_C_FUNC int __SPRT_ID(pthread_key_delete)(__SPRT_ID(pthread_key_t) key) {
	auto pool = __thread_pool::get();
	unique_lock lock(pool->mutex);

	auto it = pool->keys.find(key);
	if (it == pool->keys.end()) {
		return EINVAL;
	}

	auto data = it->second;
	pool->keys.erase(it);
	lock.unlock();

	__releaseKey(data);
	return 0;
}

__SPRT_C_FUNC void *__SPRT_ID(pthread_getspecific)(__SPRT_ID(pthread_key_t) key) {
	// we dont want to attach this thread
	auto self = thread_t::self_noattach();
	// Null storage: the thread is past TLS teardown (key destructors already ran in
	// __runthead); POSIX leaves values unspecified here, so report "no value".
	if (!self || !self->threadKeyStorage) {
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

	// Null storage: past TLS teardown — a value set now could never have its
	// destructor run, so refuse instead of resurrecting the map.
	if (!self || !self->threadKeyStorage) {
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

		data = keyIt->second;
		_atomic::fetchAdd(&data->refcount, uint32_t(1));
		lock.unlock();

		self->threadKeyStorage->emplace(key, __key_specific{data, val});
	} else {
		it->second.value = val;
	}
	return 0;
}

} // namespace sprt::_thread
