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

#include <sprt/c/__sprt_locale.h>

#include "string.h"
#include "../include/__impl_libc.h"

#if SPRT_WINDOWS
#include "windows/libc.cc"
#include "windows/libc_file_ops.cc"
#include "windows/libc_dir_ops.cc"
#endif

namespace sprt {

static uint8_t s_fdDispatchStorage[sizeof(__fd_dispatch)];
static uint8_t s_fdInitPageStorage[sizeof(__fd_page)];

static thread_local int s_errnoLocation = 0;

__SPRT_C_FUNC __SPRT_FALLBACK_ATTR(const) int *__SPRT_ID(__errno_location)(void) {
	return &s_errnoLocation;
}

__SPRT_C_FUNC __SPRT_FALLBACK_ATTR(const) int *___errno_location(void) { return &s_errnoLocation; }

__SPRT_C_FUNC void __stdio_exit_needed() { }

__libc::__libc() {
	__init_locale();

	fdDispatch = new (s_fdDispatchStorage, sprt::nothrow) __fd_dispatch;

	memset(&fdPages, 0, sizeof(fdPages));

	fdPagesAllocated = 1;
	fdPages[0] = new (s_fdInitPageStorage, sprt::nothrow) __fd_page;

	__init_default_fds(this);

	load_file_fd_ops(&fdFileOps);
	load_dir_fd_ops(&fdDirOps);

	// For debug purposes, it's better to have fd set up before exceptions
	// (priintf will work properly)
	__init_exceptions();

	localeCache.max_load_factor(2.0);

	defaultLocale = __locale_struct{
		__get_locale(__SPRT_LC_CTYPE, "C", 1),
		__get_locale(__SPRT_LC_NUMERIC, "C", 1),
		__get_locale(__SPRT_LC_TIME, "C", 1),
		__get_locale(__SPRT_LC_COLLATE, "C", 1),
		__get_locale(__SPRT_LC_MONETARY, "C", 1),
		__get_locale(__SPRT_LC_MESSAGES, "C", 1),
	};

	plocks.max_load_factor(2.0);
}

__libc::~__libc() {
	// Release locale maps
	for (auto &it : localeCache) { __free_locale(it.second); }
	localeCache.clear();

	// release memory for extra-fd
	while (fdPagesAllocated > 1) { sprt::__delete(fdPages[--fdPagesAllocated]); }

	__cleanup_exceptions();
}

struct __locale_map *__libc::get_cached_locale(const char *name, size_t len,
		const Callback<struct __locale_map *()> &cb) {
	auto nameView = StringView(name, len);
	unique_lock lock(defaultLocaleMutex);
	auto it = localeCache.find(nameView);
	if (it != localeCache.end()) {
		return it->second;
	} else {
		auto map = cb();
		return localeCache.emplace(nameView, map).first->second;
	}
}

__fd_slot *__libc::get_fd_slot(int fd) const {
	auto pageNumber = fd / FDS_PER_PAGE;
	if (pageNumber >= fdPagesAllocated) {
		return nullptr;
	}

	return &fdPages[pageNumber]->fds[fd % FDS_PER_PAGE];
}

void *__libc::get_fd_handle(int fd) const {
	auto pageNumber = fd / FDS_PER_PAGE;
	if (pageNumber >= fdPagesAllocated) {
		return nullptr;
	}

	return fdPages[pageNumber]->fds[fd % FDS_PER_PAGE].handle;
}

int __libc::create_fd(void *handle, const __fd_ops *ops, uint32_t flags, uint32_t mode) {
	auto fd = allocate_fd();
	if (fd == -1) {
		return fd;
	}

	auto pageNumber = fd / FDS_PER_PAGE;
	fdPages[pageNumber]->fds[fd % FDS_PER_PAGE] = __fd_slot{
		handle,
		ops,
		flags,
		mode,
	};
	return fd;
}

int __libc::allocate_fd() {
	unique_lock lock(fdMutex);
	size_t ret = fdDispatch->bits.find_first_not_set();
	if (ret >= fdDispatch->bits.size()) {
		__sprt_errno = EMFILE;
		return -1;
	}

	fdDispatch->bits.set(ret);

	auto pageNumber = ret / FDS_PER_PAGE;
	while (pageNumber >= fdPagesAllocated) {
		fdPages[fdPagesAllocated] = new (sprt::nothrow) __fd_page;
		if (!fdPages[fdPagesAllocated]) {
			__sprt_errno = ENOMEM;
			return -1;
		}
		++fdPagesAllocated;
	}
	return int(ret);
}

void __libc::release_fd(int fd) {
	unique_lock lock(fdMutex);
	fdDispatch->bits.reset(fd);
}

__SPRT_C_FUNC
int __SPRT_ID(sprt_plock_lock)(__SPRT_ID(sprt_plock_t) lock, __SPRT_ID(sprt_lock_flags_t) flags,
		__SPRT_ID(pid_t) * tidPtr) {
	auto libc = __libc::get();

	unique_lock ulock(libc->plockMutex);

	auto it = libc->plocks.find(lock);
	if (it == libc->plocks.end()) {
		it = libc->plocks.emplace(lock).first;
		it->second.refcount = 1;
	} else {
		++it->second.refcount;
	}

	ulock.unlock();

	__sprt_sprt_rlock_t tid;
	*rmutex_base::getNativeValue(tid) = __sprt_gettid();

	auto res = rmutex_base::_lock<sprt_rlock_wait, nullptr,
			bool(SPRT_RLOCK_PI_REQUIRES_EXTENDED_CALL)>(it->second.data.value, tid, 0,
			SPRT_LOCK_FLAG_PI);
	switch (res) {
	case Status::Ok:
	case Status::Propagate: ++it->second.data.counter; break;
	default: return -1; break;
	}

	if (it->second.flags & SPRT_LOCK_FLAG_RESOURCE_DIED) {
		sprt_plock_unlock(lock, SPRT_LOCK_FLAG_RESOURCE_DIED, tidPtr);
		__sprt_errno = EOWNERDEAD;
		return -1;
	}
	if (tidPtr != nullptr) {
		*tidPtr = (*rmutex_base::getNativeValue(tid)) & rmutex_base::THREAD_ID_MASK;
	}
	return 0;
}

__SPRT_C_FUNC
int __SPRT_ID(sprt_plock_trylock)(__SPRT_ID(sprt_plock_t) lock, __SPRT_ID(sprt_lock_flags_t),
		__SPRT_ID(pid_t) * tidPtr) {
	auto libc = __libc::get();

	unique_lock ulock(libc->plockMutex);

	auto it = libc->plocks.find(lock);
	if (it == libc->plocks.end()) {
		it = libc->plocks.emplace(lock).first;
		it->second.refcount = 1;

		// It's a new lock, this should return immediately
		__sprt_sprt_rlock_t tid;
		*rmutex_base::getNativeValue(tid) = __sprt_gettid();

		auto res = rmutex_base::_lock<sprt_rlock_wait, nullptr,
				bool(SPRT_RLOCK_PI_REQUIRES_EXTENDED_CALL)>(it->second.data.value, tid, 0,
				SPRT_LOCK_FLAG_PI);
		switch (res) {
		case Status::Ok:
		case Status::Propagate: ++it->second.data.counter; break;
		default: return -1; break;
		}
		if (tidPtr != nullptr) {
			*tidPtr = (*rmutex_base::getNativeValue(tid)) & rmutex_base::THREAD_ID_MASK;
		}
		return 0;
	} else {
		__sprt_sprt_rlock_t tid;
		*rmutex_base::getNativeValue(tid) = __sprt_gettid();

		auto res = rmutex_base::_try_lock<sprt_rlock_try_wait>(it->second.data.value, tid,
				SPRT_LOCK_FLAG_PI);
		switch (res) {
		case Status::Ok:
		case Status::Propagate:
			++it->second.refcount;
			ulock.unlock();

			++it->second.data.counter;
			if (it->second.flags & SPRT_LOCK_FLAG_RESOURCE_DIED) {
				sprt_plock_unlock(lock, SPRT_LOCK_FLAG_RESOURCE_DIED, tidPtr);
				__sprt_errno = EOWNERDEAD;
				return -1;
			}
			if (tidPtr != nullptr) {
				*tidPtr = (*rmutex_base::getNativeValue(tid)) & rmutex_base::THREAD_ID_MASK;
			}
			return 0;
			break;
		default: break;
		}
	}
	__sprt_errno = 0;
	return -1;
}

__SPRT_C_FUNC
int __SPRT_ID(sprt_plock_unlock)(__SPRT_ID(sprt_plock_t) lock, __SPRT_ID(sprt_lock_flags_t) flags,
		__SPRT_ID(pid_t) * tidPtr) {

	auto doUnlock = [&](rmutex_base::value_type &data, uint32_t *counter) {
		rmutex_base::tid_type tidWithPrio = __sprt_gettid();
		rmutex_base::value_type zero = {0};

		rmutex_base::value_type expected;
		*rmutex_base::getNativeValue(expected) =
				_atomic::loadRel(rmutex_base::getNativeValue(data));

		if (counter && *counter <= 0) {
			return Status::ErrorNotPermitted;
		}

		if ((*rmutex_base::getNativeValue(expected) & rmutex_base::THREAD_ID_MASK)
				!= (tidWithPrio & rmutex_base::THREAD_ID_MASK)) {
			return Status::ErrorNotPermitted;
		}

		if (counter && --*counter > 0) {
			// some recursive locks still in place
			return Status::Propagate;
		}

		if (tidPtr) {
			*tidPtr = 0;
		}

		// We check if we already know about the waiting threads.
		// If we don’t know, then we try to atomically unlock the mutex.
		bool unlocked = false;
		if ((*rmutex_base::getNativeValue(expected) & rmutex_base::WAITERS_BIT) != 0
				|| !(unlocked = _atomic::compareSwap(rmutex_base::getNativeValue(data),
							 rmutex_base::getNativeValue(expected),
							 *rmutex_base::getNativeValue(zero)))) {
			// Now we know for sure that there are waiting threads
			if (!unlocked) {
				// CAS failed (Waiters flag was added)
				// or WAITERS_BIT was already set, CAS was not performed
				//
				// Force-set data to 0;
				_atomic::storeSeq(rmutex_base::getNativeValue(data),
						*rmutex_base::getNativeValue(zero));
			}
			if (sprt_rlock_wake(&data, 0) != 0) {
				return status::errnoToStatus(__sprt_errno);
			}
			return Status::Ok;
		}
		return Status::Done;
	};

	auto libc = __libc::get();

	unique_lock ulock(libc->plockMutex);

	auto it = libc->plocks.find(lock);
	if (it == libc->plocks.end()) {
		__sprt_errno = ESRCH;
		return -1;
	}

	auto recount = it->second.refcount;
	if (recount == 1) {
		doUnlock(it->second.data.value, &it->second.data.counter);
		libc->plocks.erase(it);
	} else {
		if (flags & SPRT_LOCK_FLAG_RESOURCE_DIED) {
			it->second.flags |= SPRT_LOCK_FLAG_RESOURCE_DIED;
		}
		--it->second.refcount;
		doUnlock(it->second.data.value, &it->second.data.counter);
	}
	return 0;
}

} // namespace sprt
