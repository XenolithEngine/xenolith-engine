/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include <sprt/runtime/mem/context.h>
#include <sprt/runtime/mem/function.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/new.h>

namespace sprt::memory::pool {

static constexpr size_t SP_ALLOC_STACK_SIZE = 4'097;

class AllocStack {
public:
	struct Info {
		pool_t *pool;
		PoolCtxInfo info;
	};

	AllocStack();

	pool_t *top() const;
	const PoolCtxInfo &info() const;
	const Info &back() const;

	void push(pool_t *, const char *source);
	void push(pool_t *, uint32_t, const void *, const char *source);
	void pop(pool_t *, const char *source);

	void foreachInfo(void *, bool (*cb)(void *, pool_t *, const PoolCtxInfo &));

protected:
	template <typename T>
	struct stack {
		size_t size = 0;
		array<T, SP_ALLOC_STACK_SIZE> data;

		bool empty() const { return size == 0; }
#if DEBUG
		void push(const T &t) {
			if (size < data.size()) {
				data[size] = t;
				++size;
			} else {
				__sprt_abort();
			}
		}
		void pop() {
			if (size > 0) {
				--size;
			} else {
				__sprt_abort();
			}
		}
		const T &get() const {
			if (size == 0) {
				__sprt_abort();
			}
			return data[size - 1];
		}
#else
		void push(const T &t) { data[size++] = t; }
		void pop() { --size; }
		const T &get() const { return data[size - 1]; }
#endif
	};

	stack<Info> _stack;
};

AllocStack::AllocStack() {
	_stack.push(Info{
		nullptr,
		{
			0,
			nullptr,
			__SPRT_FUNC,
		},
	});
}

pool_t *AllocStack::top() const { return _stack.get().pool; }

const PoolCtxInfo &AllocStack::info() const { return _stack.get().info; }

const AllocStack::Info &AllocStack::back() const { return _stack.get(); }

void AllocStack::push(pool_t *p, const char *source) {
	if (p) {
		_stack.push(Info{
			p,
			{
				0,
				nullptr,
				source,
			},
		});
	} else {
		::__sprt_abort();
	}
}
void AllocStack::push(pool_t *p, uint32_t tag, const void *ptr, const char *source) {
	if (p) {
		_stack.push(Info{
			p,
			{
				tag,
				ptr,
				source,
			},
		});
	} else {
		::__sprt_abort();
	}
}

void AllocStack::pop(pool_t *p, const char *source) {
#if DEBUG
	if (_stack.get().pool == p && (!source || _stack.get().info.source == source)) {
		_stack.pop();
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "memory", "Unbalansed pool::push found");
		::__sprt_abort();
	}
#else
	_stack.pop();
#endif
}

void AllocStack::foreachInfo(void *data, bool (*cb)(void *, pool_t *, const PoolCtxInfo &)) {
	for (size_t i = 0; i < _stack.size; ++i) {
		auto &it = _stack.data[_stack.size - 1 - i];
		if (it.pool && !cb(data, it.pool, it.info)) {
			break;
		}
	}
}

static AllocStack &get_stack() {
	static thread_local AllocStack tl_stack;
	return tl_stack;
}

pool_t *acquire() { return get_stack().top(); }

PoolCtxInfo info() { return get_stack().info(); }

void push(pool_t *p, const char *source) { return get_stack().push(p, source); }

void push(pool_t *p, uint32_t tag, const void *ptr, const char *source) {
	set_pool_info(p, tag, ptr);
	return get_stack().push(p, tag, ptr, source);
}

void pop(pool_t *p, const char *source) { return get_stack().pop(p, source); }

static Status cleanup_register_fn(void *ptr) {
	if (auto fn = (memory::function<void()> *)ptr) {
		(*fn)();
	}
	return Status::Ok;
}

void cleanup_register(pool_t *p, memory::function<void()> &&cb) {
	perform_conditional([&] {
		auto fn = new (p) memory::function<void()>(move(cb));
		pool::cleanup_register(p, fn, &cleanup_register_fn);
	}, p);
}

void pre_cleanup_register(pool_t *p, memory::function<void()> &&cb) {
	perform_conditional([&] {
		auto fn = new (p) memory::function<void()>(move(cb));
		pool::pre_cleanup_register(p, fn, &cleanup_register_fn);
	}, p);
}

} // namespace sprt::memory::pool

namespace sprt::memory {

void foreach_info(void *data, bool (*cb)(void *userdata, pool_t *, const PoolCtxInfo &)) {
	pool::get_stack().foreachInfo(data, cb);
}

} // namespace sprt::memory
