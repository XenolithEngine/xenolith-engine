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

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_limits.h>
#include <sprt/cxx/unordered_map>
#include <sprt/c/sys/__sprt_random.h>
#include <sprt/runtime/utils/base64.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/enum.h>
#include <sprt/runtime/filesystem/filepath.h>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#include "specific.h"
#include "stdlib.h"
#include "stdio.h"
#include "fcntl.h"

#include "../../include/__impl_libc.h"

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>

namespace sprt {

struct _EnvBlock {
	char *get(const char *name) {
		auto bufSize = GetEnvironmentVariableA(name, nullptr, 0);
		if (bufSize == 0) {
			return nullptr;
		}

		auto it = _envs.find(name);
		if (it == _envs.end()) {
			it = _envs.emplace(name, __local_string()).first;
		}

		it->second.resize(bufSize - 1); // strip nullptr from size

		if (GetEnvironmentVariableA(name, it->second.data(), it->second.size() + 1)
				== it->second.size() + 1) {
			return it->second.data();
		}
		return nullptr;
	}

	__local_unordered_map<__local_string, __local_string> _envs;
};

thread_local _EnvBlock tl_env;
static _EnvBlock s_env;

__SPRT_C_FUNC char *getenv(const char *name) __SPRT_NOEXCEPT {
	if (__libc::get()->mainThread == __sprt_gettid()) {
		return s_env.get(name);
	} else {
		return tl_env.get(name);
	}
}

__SPRT_C_FUNC int setenv(const char *name, const char *value, int override) __SPRT_NOEXCEPT {
	if (!override) {
		auto len = GetEnvironmentVariableA(name, nullptr, 0);
		if (len > 0) {
			return 0;
		}
	}
	if (SetEnvironmentVariableA(name, value)) {
		return 0;
	}
	__sprt_errno = platform::lastErrorToErrno(GetLastError());
	return -1;
}

__SPRT_C_FUNC int unsetenv(const char *name) __SPRT_NOEXCEPT {
	if (SetEnvironmentVariableA(name, nullptr)) {
		return 0;
	}
	__sprt_errno = platform::lastErrorToErrno(GetLastError());
	return -1;
}

template <typename Callback>
static bool __realpath_convert(StringView path, const Callback &cb) {
	wchar_t *out = nullptr;
	unicode::toUtf16([&](WideStringView str) {
		PathAllocCanonicalize((wchar_t *)str.data(), 0, &out);
	}, path);

	if (out) {
		auto attrs = GetFileAttributesW(out);
		if (attrs == INVALID_FILE_ATTRIBUTES) {
			LocalFree(out);
			return false;
		}

		if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
			HANDLE hLink = CreateFileW(out, GENERIC_READ,
					FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
					FILE_FLAG_BACKUP_SEMANTICS, NULL);
			if (hLink == INVALID_HANDLE_VALUE) {
				LocalFree(out);
				return false;
			}

			auto pathLen = GetFinalPathNameByHandleW(hLink, NULL, 0, 0);
			if (pathLen == 0) {
				LocalFree(out);
				__sprt_errno = platform::lastErrorToErrno(GetLastError());
				return -1;
			}

			auto buf = __sprt_typed_malloca(wchar_t, pathLen + 1);

			auto writtenLen = GetFinalPathNameByHandleW(hLink, buf, pathLen + 1, 0);
			if (writtenLen == 0) {
				LocalFree(out);
				__sprt_freea(buf);
				__sprt_errno = platform::lastErrorToErrno(GetLastError());
				return -1;
			}

			WideStringView wstr((char16_t *)buf, writtenLen);
			auto outLen = unicode::getUtf8Length(wstr);
			auto out = __sprt_typed_malloca(char, outLen + 1);

			size_t ulen = 0;
			unicode::toUtf8(out, outLen, wstr, &ulen);

			auto len = __sprt_fpath_to_posix(out, ulen, out, outLen + 1);

			cb(StringView(out, len));

			__sprt_freea(out);
			__sprt_freea(buf);
			CloseHandle(hLink);
		} else {
			unicode::toUtf8([&](StringView str) {
				// convert in place
				auto len = __sprt_fpath_to_posix(str.data(), str.size(), (char *)str.data(),
						str.size() + 1);

				cb(StringView(str.data(), len));
			}, WideStringView((char16_t *)out));
		}
		LocalFree(out);
		return true;
	}

	return false;
}

static char *__realpath_write(StringView str, char *out) {
	if (!out) {
		auto ret = (char *)__sprt_malloc(str.size() + 1);
		memcpy(ret, str.data(), str.size() + 1);
		return ret;
	} else {
		if (str.size() + 1 < __SPRT_PATH_MAX) {
			memcpy(out, str.data(), str.size() + 1);
		} else {
			memcpy(out, str.data(), __SPRT_PATH_MAX - 1);
			out[__SPRT_PATH_MAX - 1] = 0;
		}
		return out;
	}
}

__SPRT_C_FUNC char *realpath(const char *__SPRT_RESTRICT ipath,
		char *__SPRT_RESTRICT out) __SPRT_NOEXCEPT {
	char *ret = nullptr;
	StringView path(ipath);
	if (__sprt_fpath_is_posix(path.data(), path.size())) {
		filepath::reconstructPath([&](StringView reconstructed) {
			auto buflen = sprt::max(size_t(8), reconstructed.size() + 1);
			auto buf = __sprt_typed_malloca(char, buflen);

			auto len =
					__sprt_fpath_to_native(reconstructed.data(), reconstructed.size(), buf, buflen);
			__realpath_convert(StringView(buf, len), [&](StringView str) {
				ret = __realpath_write(str, out); //
			});
			__sprt_freea(buf);
		}, path);
	} else {
		__realpath_convert(path, [&](StringView str) {
			ret = __realpath_write(str, out); //
		});
	}
	return ret;
}

__SPRT_C_FUNC void *local_alloc(size_t value) __SPRT_NOEXCEPT {
	return (void *)HeapAlloc(GetProcessHeap(), 0, value);
}

__SPRT_C_FUNC void local_free(void *ptr, size_t value) __SPRT_NOEXCEPT {
	HeapFree(GetProcessHeap(), 0, ptr);
}

} // namespace sprt
