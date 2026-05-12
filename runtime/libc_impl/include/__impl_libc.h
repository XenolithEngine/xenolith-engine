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

#ifndef RUNTIME_FREESTANDING_INCLUDE_IMPL_LIBC_H_
#define RUNTIME_FREESTANDING_INCLUDE_IMPL_LIBC_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/sys/__sprt_stat.h>
#include <sprt/c/bits/__sprt_size_t.h>

#include <sprt/cxx/mutex>
#include <sprt/cxx/unordered_map>
#include <sprt/cxx/bitset>
#include <sprt/runtime/stringview.h>

#include <sprt/c/__sprt_dirent.h>

#include "unistd.h"
#include "sys/sprt.h"
#include "sys/stat.h"

namespace sprt {

// static limit of 32k
static constexpr size_t MAX_FDS = 1'024 * 32;
static constexpr size_t FD_MEMORY_PAGE_SIZE = 1'024 * 16;

struct __locale_map;

struct __locale_struct {
	const struct __locale_map *cat[6];
};

struct __fd_dispatch {
	sprt::bitset<MAX_FDS> bits;
};

struct __fd_ops;

struct __fd_slot {
	void *handle;
	const __fd_ops *ops;
	uint64_t padding;
	uint32_t flags;
	uint32_t mode;
};

enum class __fd_ops_mask : uint32_t {
	none = 0,
	opendir = 1 << 0,
};

SPRT_DEFINE_ENUM_AS_MASK(__fd_ops_mask)

enum class __fd_ctl_mode {
	fnctl,
	ioctl,
};

struct __fd_ops {
	__fd_ops_mask mask = __fd_ops_mask::none;

	ssize_t (*fo_read)(__fd_slot *fp, void *buf, size_t nbyte, off64_t *offset,
			uint32_t flags) = nullptr;
	ssize_t (*fo_write)(__fd_slot *fp, const void *buf, size_t nbyte, off64_t *offset,
			uint32_t flags) = nullptr;
	int (*fo_ioctl)(__fd_slot *fp, int fd, int cmd, intptr_t arg, __fd_ctl_mode) = nullptr;
	int (*fo_dup)(__fd_slot *fp, int *target, uint32_t flags) = nullptr;
	int (*fo_close)(__fd_slot *fp) = nullptr;

	ssize_t (*fo_readv)(__fd_slot *fp, const iovec *iov, int iovcnt) = nullptr;
	ssize_t (*fo_writev)(__fd_slot *fp, const iovec *iov, int iovcnt) = nullptr;
	off_t (*fo_seek)(__fd_slot *fp, off_t, int) = nullptr;
	int (*fo_stat)(__fd_slot *fp, struct __SPRT_STAT_NAME *__stat) = nullptr;
	int (*fo_chmod)(__fd_slot *fp, mode_t mode) = nullptr;
	int (*fo_utimens)(__fd_slot *fp, const struct __SPRT_TIMESPEC_NAME *times) = nullptr;
};

struct __fd_page {
	sprt::array<__fd_slot, FD_MEMORY_PAGE_SIZE / sizeof(__fd_slot) > fds;
};

static_assert(sizeof(__fd_dispatch) == 4'096);

// 16k-page optimized
static_assert(sizeof(__fd_page) == FD_MEMORY_PAGE_SIZE);

struct __libc_plock {
	__rmutex_data data;
	uint32_t refcount = 0;
	uint32_t flags = 0;
};

struct __libc {
	static constexpr size_t FD_PAGES_COUNT = MAX_FDS / (1'024 * 16 / sizeof(__fd_slot));
	static constexpr size_t FDS_PER_PAGE = FD_MEMORY_PAGE_SIZE / sizeof(__fd_slot);

	static constexpr size_t STDIN_FD = 0;
	static constexpr size_t STDOUT_FD = 1;
	static constexpr size_t STDERR_FD = 2;

	static __libc *get();

	static void load_file_fd_ops(__fd_ops *);
	static void load_dir_fd_ops(__fd_ops *);

	mutable mutex defaultLocaleMutex;
	struct __locale_struct defaultLocale;

	uint32_t euid = 0;
	uint32_t egid = 0;
	bool isAppContainer = false;

	__malloc_unordered_map<StringView, __locale_map *> localeCache;

	mutex fdMutex;
	__fd_dispatch *fdDispatch;

	size_t fdPagesAllocated;
	sprt::array<__fd_page *, FD_PAGES_COUNT> fdPages;

	__fd_ops fdFileOps;
	__fd_ops fdDirOps;

	mutable mutex plockMutex;
	__malloc_unordered_map<sprt_plock_t, __libc_plock> plocks;

	__libc();
	~__libc();

	struct __locale_map *get_cached_locale(const char *, size_t len,
			const Callback<struct __locale_map *()> &);

	__fd_slot *get_fd_slot(int) const;
	void *get_fd_handle(int) const;

	int create_fd(void *, const __fd_ops *, uint32_t flags, uint32_t mode);
	int allocate_fd();
	void release_fd(int);
};

void __init_locale();
__locale_map *__get_default_locale();
__locale_map *__get_locale(int cat, const char *, size_t);
void __free_locale(__locale_map *);

bool __init_exceptions();
void __cleanup_exceptions();

void __init_default_fds(__libc *);

// Note that maps are cached, immutable, and persistent - no locking required after acquisition
const __locale_map *__get_effective_locale_map(int);

wchar_t __towlower_l(wchar_t ch, const __locale_map *locMap);
wchar_t __towupper_l(wchar_t ch, const __locale_map *locMap);

int __wcscmp_l(const wchar_t *, const wchar_t *, const __locale_map *);
int __wcsncmp_l(const wchar_t *, const wchar_t *, __sprt_size_t, const __locale_map *);

int __wcscasecmp_l(const wchar_t *, const wchar_t *, const __locale_map *);
int __wcsncasecmp_l(const wchar_t *, const wchar_t *, __sprt_size_t, const __locale_map *);
int __wcscoll_l(const wchar_t *, const wchar_t *, const __locale_map *);
size_t __wcsxfrm_l(wchar_t *__restrict, const wchar_t *__restrict, __sprt_size_t,
		const __locale_map *);

int __scandir(__SPRT_ID(DIR) * d, struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **));

} // namespace sprt

#endif // RUNTIME_FREESTANDING_INCLUDE_IMPL_LIBC_H_
