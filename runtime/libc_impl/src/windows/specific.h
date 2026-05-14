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

#ifndef RUNTIME_FREESTANDING_SRC_WINDOWS_MALLOCA_STRING_H_
#define RUNTIME_FREESTANDING_SRC_WINDOWS_MALLOCA_STRING_H_

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/basic_types.h>
#include <sprt/runtime/stringview.h>
#include <sprt/c/sys/__sprt_stat.h>
#include <sprt/c/sys/__sprt_mman.h>

namespace sprt::platform {

struct OpenInfo {
	DWORD dwDesiredAccess = 0;
	DWORD dwShareMode = 0;
	DWORD dwFlagsAndAttributes = 0;
	DWORD dwCreationDisposition = 0;

	// @AI-generated
	OpenInfo(int __flags, int __mode);
};

template <typename Result, typename Callback>
static inline auto performWithNativePath(const char *path, const Callback &cb,
		const Result &error) {
	static_assert(is_invocable_v<Callback, const char *>);

	auto pathlen = path ? __constexpr_strlen(path) : 0;
	auto isNative = __sprt_fpath_is_native(path, pathlen);
	if (!path || isNative) {
		return cb(path);
	} else {
		auto buflen = sprt::max((__SPRT_ID(size_t))8, pathlen + 1);
		auto buf = __sprt_typed_malloca(char, buflen);
		if (__sprt_fpath_to_native(path, pathlen, buf, buflen) > 0) {
			return cb(buf);
		}
		__sprt_freea(buf);
	}
	*__sprt___errno_location() = EINVAL;
	return error;
}

template <typename Result, typename Callback>
static inline auto performWithPosixePath(const char *path, const Callback &cb,
		const Result &error) {
	static_assert(is_invocable_v<Callback, const char *>);

	auto pathlen = path ? __constexpr_strlen(path) : 0;
	auto isPosix = __sprt_fpath_is_posix(path, pathlen);
	if (!path || isPosix) {
		return cb(path);
	} else {
		auto buf = __sprt_typed_malloca(char, pathlen + 1);
		if (__sprt_fpath_to_posix(path, pathlen, buf, pathlen + 1) > 0) {
			return cb(buf);
		}
		__sprt_freea(buf);
	}
	*__sprt___errno_location() = EINVAL;
	return error;
}

template < typename Callback>
static inline auto performBinaryMode(const char *mode, const Callback &cb) {
	static_assert(is_invocable_v<Callback, const char *>);
	StringView smode(mode);
	if (smode.find('b') == Max<size_t>) {
		// add 'b' to mode string
		auto modeLen = __constexpr_strlen(mode) + 2;
		auto modeBuf = __sprt_typed_malloca(char, modeLen);
		auto target = modeBuf;

		target = strappend(target, &modeLen, smode.data(), smode.size());
		target = strappend(target, &modeLen, "b", 1);

		cb(modeBuf);
	} else {
		cb(mode);
	}
}

static inline struct __SPRT_TIMESPEC_NAME __toTimespec(const FILETIME &ft) {
	constexpr unsigned __int64 TICKS_PER_SEC = 10'000'000ULL;
	constexpr unsigned __int64 EPOCH_DIFFERENCE = 11'644'473'600ULL;

	ULARGE_INTEGER ull;
	ull.LowPart = ft.dwLowDateTime;
	ull.HighPart = ft.dwHighDateTime;

	struct __SPRT_TIMESPEC_NAME ts;
	ts.tv_sec = (time_t)((ull.QuadPart / TICKS_PER_SEC) - EPOCH_DIFFERENCE);
	ts.tv_nsec = (long)((ull.QuadPart % TICKS_PER_SEC) * 100);
	return ts;
}

static inline LARGE_INTEGER __toFiletime(const struct __SPRT_TIMESPEC_NAME *ts) {
	constexpr unsigned __int64 EPOCH_DIFF_NS100 = (11'644'473'600ULL * 10'000'000ULL);

	LARGE_INTEGER ret;
	unsigned __int64 total_ns100 = (unsigned __int64)ts->tv_sec * 10'000'000ULL + ts->tv_nsec;
	total_ns100 += EPOCH_DIFF_NS100;
	ret.QuadPart = total_ns100;
	return ret;
}

inline DWORD __getProtectFlags(int prot) {
	DWORD flProtect = 0;
	if (prot & __SPRT_PROT_EXEC) {
		if (prot & __SPRT_PROT_WRITE) {
			flProtect = PAGE_EXECUTE_READWRITE;
		} else if (prot & __SPRT_PROT_READ) {
			flProtect = PAGE_EXECUTE_READ;
		} else {
			flProtect |= PAGE_EXECUTE;
		}
	} else if (prot & __SPRT_PROT_WRITE) {
		flProtect = PAGE_READWRITE;
	} else if (prot & __SPRT_PROT_READ) {
		flProtect = PAGE_READONLY;
	} else {
		flProtect = PAGE_NOACCESS;
	}
	return flProtect;
}

bool isAppContainer();

int lastErrorToErrno(unsigned long winerr);

// should construct path, that can be open for specific FdHandleType
bool openAtPath(int fd, const char *path, const callback<void(const char *, size_t)> &);

uint32_t getRid(void *sid);

int hstat(HANDLE h, struct __SPRT_STAT_NAME *__stat);

int hutimens(HANDLE hFile, const struct __SPRT_TIMESPEC_NAME *times);

struct __wstring {
	static wchar_t *toWStringBuf(wchar_t *buf, const char *source) {
		size_t len = 0;
		unicode::toUtf16((char16_t *)buf, Max<size_t>, StringView(source), &len);
		buf[len] = 0;
		return buf;
	};

	static wchar_t *toWStringBuf(wchar_t *buf, const char *source, size_t sourceLen) {
		size_t len = 0;
		unicode::toUtf16((char16_t *)buf, Max<size_t>, StringView(source, sourceLen), &len);
		buf[len] = 0;
		return buf;
	};

	static char *toStringBuf(char *buf, const wchar_t *source) {
		size_t len = 0;
		unicode::toUtf8(buf, Max<size_t>, WideStringView((char16_t *)source), &len);
		buf[len] = 0;
		return buf;
	};

	static char *toStringBuf(char *buf, const wchar_t *source, size_t sourceLen) {
		size_t len = 0;
		unicode::toUtf8(buf, Max<size_t>, WideStringView((char16_t *)source, sourceLen), &len);
		buf[len] = 0;
		return buf;
	};

	~__wstring() {
		if (data) {
			__sprt_free(data);
			data = nullptr;
		}
	}

	__wstring(StringView str) {
		auto wlen = unicode::getUtf16Length(str);
		data = (wchar_t *)__sprt_malloc((wlen + 1) * sizeof(wchar_t));

		auto len = wlen;
		auto buf = (char16_t *)data;

		unicode::toUtf16(buf, len + 1, str, &len);
		buf[len] = 0;
		size = len;
	}
	__wstring(const char *str, size_t len) : __wstring(StringView(str, len)) { }

	__wstring(const char *str) : __wstring(str, __constexpr_strlen(str)) { }

	operator wchar_t *() const { return data; }

	size_t size = 0;
	wchar_t *data = nullptr;
};

#define __MALLOCA_WSTRING(Str) ::sprt::platform::__wstring::toWStringBuf(\
	__sprt_typed_malloca(wchar_t, ::sprt::unicode::getUtf16Length(StringView(Str)) + 1), Str)

#define __MALLOCA_WSTRINGS(Str, Size) ::sprt::platform::__wstring::toWStringBuf(\
	__sprt_typed_malloca(wchar_t, ::sprt::unicode::getUtf16Length(StringView(Str, Size)) + 1), Str, Size)


#define __MALLOCA_STRING(Str) ::sprt::platform::__wstring::toStringBuf(\
	__sprt_typed_malloca(char, ::sprt::unicode::getUtf8Length(WideStringView((char16_t *)Str)) + 1), Str)

#define __MALLOCA_STRINGS(Str, Size) ::sprt::platform::__wstring::toStringBuf(\
	__sprt_typed_malloca(char, ::sprt::unicode::getUtf8Length(WideStringView((char16_t *)Str, Size)) + 1), Str, Size)

} // namespace sprt::platform

#endif // RUNTIME_FREESTANDING_SRC_WINDOWS_MALLOCA_STRING_H_
