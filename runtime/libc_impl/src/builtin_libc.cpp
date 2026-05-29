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

__plock_storage *__libc_get_plock_storage() { return &__libc::get()->plockStorage; }

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

	plockStorage.plocks.max_load_factor(2.0);

	mainThread = __sprt_gettid();
}

__libc::~__libc() {
	// Release locale maps
	for (auto &it : localeCache) {
		__free_locale(it.second); //
	}
	localeCache.clear();

	for (auto &it : tzCache) {
		__sprt_local_free((void *)it.data(), it.size() + 1); //
	}
	tzCache.clear();

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

const char *__libc::preserve_tz_name(StringView name) {
	unique_lock<mutex> lock(tzMutex);

	auto it = tzCache.find(name);
	if (it != tzCache.end()) {
		return it->data();
	}

	auto data = (char *)__sprt_local_alloc(name.size() + 1);
	memcpy(data, name.data(), name.size());
	data[name.size()] = 0;

	return tzCache.emplace(StringView(data, name.size())).first->data();
}

} // namespace sprt
