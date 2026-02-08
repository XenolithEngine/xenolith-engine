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
#include <sprt/c/sys/__sprt_random.h>
#include <sprt/runtime/mem/map.h>
#include <sprt/runtime/mem/string.h>
#include <sprt/runtime/base64.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/enum.h>
#include <sprt/runtime/filesystem/filepath.h>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdlib.h>

#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>
#include <io.h>
#include <stdio.h>
#include <pathcch.h>

#include "private/SPRTSpecific.h"

namespace sprt {

struct _EnvBlock {
	_EnvBlock() { }

	char *get(const char *name) {
		auto bufSize = GetEnvironmentVariableA(name, nullptr, 0);
		if (bufSize == 0) {
			return nullptr;
		}

		auto it = _envs.find(name);
		if (it == _envs.end()) {
			it = _envs.emplace(name, memory::dynstring()).first;
		}

		it->second.resize(bufSize - 1); // strip nullptr from size

		if (GetEnvironmentVariableA(name, it->second.data(), it->second.size() + 1)
				== it->second.size() + 1) {
			return it->second.data();
		}
		return nullptr;
	}

	memory::dynmap<memory::dynstring, memory::dynstring> _envs;
};

thread_local _EnvBlock tl_env;

static char *getenv(const char *name) { return tl_env.get(name); }

static int getenv_s(size_t *pReturnValue, char *buffer, size_t numberOfElements,
		const char *varname) {
	auto len = GetEnvironmentVariableA(varname, buffer, numberOfElements);
	if (pReturnValue) {
		*pReturnValue = len;
	}
	if (len == 0) {
		return ESRCH;
	} else if (len > numberOfElements) {
		return ERANGE;
	}
	return 0;
}

static int setenv(const char *name, const char *value, int override) {
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

static int unsetenv(const char *name) {
	if (SetEnvironmentVariableA(name, nullptr)) {
		return 0;
	}
	__sprt_errno = platform::lastErrorToErrno(GetLastError());
	return -1;
}

static int posix_memalign(void **memptr, size_t alignment, size_t size) {
	if (!memptr || (alignment % sizeof(void *)) != 0 || __builtin_popcount(alignment) != 1) {
		return EINVAL;
	}

	int saved_errno = errno;
	*memptr = _aligned_malloc(size, alignment);
	if (*memptr == NULL) {
		errno = ENOMEM;
		return errno;
	}
	errno = saved_errno;
	return 0;
}

template <typename Callback>
static bool __mktmppath(char *itpl, const Callback &cb) {
	static constexpr size_t PathBufferLen = MAX_PATH + 2;

	wchar_t tmpDirPath[PathBufferLen] = {0};
	wchar_t tmpFilePath[PathBufferLen] = {0};

	auto dirLength = GetTempPathW(PathBufferLen, tmpDirPath);
	if (dirLength < 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto wtpl = __MALLOCA_WSTRING(itpl);

	StringViewBase<wchar_t> tpl(wtpl);
	if (!tpl.ends_with(L"XXXXXX")) {
		__sprt_freea(wtpl);
		__sprt_errno = EINVAL;
		return -1;
	}

	uint8_t randomBytes[6];
	char b64Bytes[8];

	__sprt_getrandom(randomBytes, 6, __SPRT_GRND_RANDOM);

	base64url::encode(randomBytes, 6, b64Bytes, 8);
	memcpy((char *)(tpl.data() + tpl.size() - 6), b64Bytes, 6);

	size_t tmpPrefixPathBufLen = PathBufferLen;
	auto target = tmpFilePath;

	target = strappend(target, &tmpPrefixPathBufLen, tmpDirPath, dirLength);
	target = strappend(target, &tmpPrefixPathBufLen, L"\\", 1);
	target = strappend(target, &tmpPrefixPathBufLen, tpl.data(), tpl.size());
	if (tmpPrefixPathBufLen == 0) {
		__sprt_errno = ERANGE;
		return false;
	}

	cb(tmpFilePath, target - tmpFilePath);

	__sprt_freea(wtpl);
	return true;
}

static int mkostemp(char *itpl, int _flags) {
	int fd = -1;
	if (!__mktmppath(itpl, [&](const wchar_t *path, size_t pathLength) {
		auto flags = _O_CREAT | _O_SHORT_LIVED | _O_TEMPORARY | _O_RDWR | _O_BINARY;
		for (auto f : sprt::flags(uint32_t(_flags))) {
			switch (f) {
			case __SPRT_O_APPEND: flags |= _O_APPEND; break;
			case __SPRT_O_CLOEXEC: flags |= _O_NOINHERIT; break;
			case __SPRT_O_SYNC: break;
			}
		}

		_wsopen_s(&fd, path, flags, _SH_DENYRW, _S_IREAD | _S_IWRITE);
	})) {
		return -1;
	}

	return fd;
}

static int mkstemp(char *itpl) { return mkostemp(itpl, 0); }

static char *mkdtemp(char *itpl) {
	char *ret = nullptr;
	if (!__mktmppath(itpl, [&](const wchar_t *path, size_t pathLength) {
		if (!CreateDirectoryW(path, nullptr)) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
		} else {
			ret = itpl;
		}
	})) {
		return nullptr;
	}
	return ret;
}

// from musl-libc
static int getsubopt(char **opt, char *const *keys, char **val) {
	char *s = *opt;
	int i;

	*val = nullptr;
	*opt = strchr(s, ',');
	if (*opt) {
		*(*opt)++ = 0;
	} else {
		*opt = s + strlen(s);
	}

	for (i = 0; keys[i]; i++) {
		size_t l = strlen(keys[i]);
		if (strncmp(keys[i], s, l)) {
			continue;
		}
		if (s[l] == '=') {
			*val = s + l + 1;
		} else if (s[l]) {
			continue;
		}
		return i;
	}
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
		auto ret = (char *)malloc(str.size() + 1);
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

static char *realpath(const char *__SPRT_RESTRICT ipath, char *__SPRT_RESTRICT out) {
	char *ret = nullptr;
	StringView path(ipath);
	if (__sprt_fpath_is_posix(path.data(), path.size())) {
		filepath::reconstructPath([&](StringView reconstructed) {
			auto buflen = max(8, reconstructed.size() + 1);
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

} // namespace sprt
