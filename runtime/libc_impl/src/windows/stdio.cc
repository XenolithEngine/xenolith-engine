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

#include <sprt/runtime/string.h>
#include <sprt/runtime/stringview.h>
#include <sprt/cxx/cctype>
#include <sprt/c/__sprt_errno.h>

#include "specific.h"
#include "stdlib.h"
#include "unistd.h"
#include "stdio.h"
#include "errno.h"

#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>

namespace sprt {

template <typename Char>
static int __is_exact_windows_native(StringViewBase<Char> source) {
	// Any path, that starts with \ is windows-native
	if (source.starts_with(Char('\\'))) {
		return 1;
	}

	// C:Directory, C:\Directory, C:/Directory is Windows native
	if (source.size() > 1 && source.template is<typename StringViewBase<Char>::LatinUppercase>()
			&& source.at(1) == Char(':')) {
		return 1;
	}

	// Any path with \ in it is windows native
	source.template readUntil<typename StringViewBase<Char>::template Chars<'\\'>>();
	if (source.is('\\')) {
		return 1;
	}

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(fpath_is_native)(const char *path, __SPRT_ID(size_t) pathSize) {
	if (__is_exact_windows_native(StringView(path, pathSize))) {
		return 1;
	}

	// Path without / also windows-native (no need to convert it), but also POSIX

	StringView source(path, pathSize);
	source.readUntil<StringView::Chars<'/'>>();
	if (source.is('/')) {
		// Actually, we can pass paths with / to WinAPI, but we can not detect other special convertion cases with it
		return 0;
	}
	return 1;
}

__SPRT_C_FUNC int __SPRT_ID(fpath_is_posix)(const char *path, __SPRT_ID(size_t) pathSize) {
	if (__is_exact_windows_native(StringView(path, pathSize))) {
		return 0;
	}

	// All that is not exact windows is POSIX
	return 1;
}

__SPRT_C_FUNC int __SPRT_ID(
		wfpath_is_native)(const __SPRT_ID(wchar_t) * path, __SPRT_ID(size_t) pathSize) {
	if (__is_exact_windows_native(StringViewBase<char16_t>((char16_t *)path, pathSize))) {
		return 1;
	}

	// Path without / also windows-native (no need to convert it), but also POSIX

	StringViewBase<char16_t> source((const char16_t *)path, pathSize);
	source.readUntil<StringViewBase<char16_t>::Chars<'/'>>();
	if (source.is('/')) {
		// Actually, we can pass paths with / to WinAPI, but we can not detect other special convertion cases with it
		return 0;
	}
	return 1;
}

__SPRT_C_FUNC int __SPRT_ID(
		wfpath_is_posix)(const __SPRT_ID(wchar_t) * path, __SPRT_ID(size_t) pathSize) {
	if (__is_exact_windows_native(StringViewBase<char16_t>((char16_t *)path, pathSize))) {
		return 0;
	}

	// All that is not exact windows is POSIX
	return 1;
}

template <typename Char>
size_t fpath_to_posix(const Char *path, size_t pathSize, Char *buf, size_t bufSize) {
	if (bufSize < pathSize) {
		return 0;
	}

	auto target = buf;
	StringViewBase<Char> source(path, pathSize);
	// Windows 10 version 1803 is required for the runtime, we can support long paths without
	// DOS device prefix, just skip it
	if constexpr (sizeof(Char) == 2) {
		if (source.starts_with(u"\\\\?\\")) {
			source += __constexpr_strlen(u"\\\\?\\");
		}
	} else {
		if (source.starts_with("\\\\?\\")) {
			source += __constexpr_strlen("\\\\?\\");
		}
	}

	auto sep = Char('/');
	if (source.size() > 2 && source.template is<typename StringViewBase<Char>::Latin>()
			&& source.at(1) == Char(':') && source.at(2) == Char('\\')) {
		auto driveLetter = tolower_c(source.at(0));
		source += 2;
		target = strappend<Char, false>(target, &bufSize, &sep, 1);
		target = strappend<Char, false>(target, &bufSize, &driveLetter, 1);
	} else if (source.size() > 1 && source.template is<typename StringViewBase<Char>::Latin>()
			&& source.at(1) == Char(':')) {
		auto driveLetter = tolower_c(source.at(0));
		source += 2;
		target = strappend<Char, false>(target, &bufSize, &sep, 1);
		target = strappend<Char, false>(target, &bufSize, &driveLetter, 1);
	}

	while (!source.empty()) {
		if (source.is('\\')) {
			auto c = Char('/');
			source.template skipChars<typename StringViewBase<Char>::template Chars<'\\'>>();
			target = strappend<Char, false>(target, &bufSize, &c, 1);
		}
		auto component =
				source.template readUntil<typename StringViewBase<Char>::template Chars<'\\'>>();
		if (!component.empty()) {
			target = strappend<Char, false>(target, &bufSize, component.data(), component.size());
		}
	}
	if (bufSize > 0) {
		target[0] = 0;
	}
	return target - buf;
}

template <typename Char>
size_t fpath_to_native(const Char *path, size_t pathSize, Char *buf, size_t bufSize) {
	if (bufSize < pathSize) {
		return 0;
	}

	auto target = buf;
	StringViewBase<Char> source(path, pathSize);
	if constexpr (sizeof(Char) == 2) {
		if (source == u"/") {
			target = strappend<Char>(target, &bufSize, u"C:", 2);
			return target - buf;
		}
	} else {
		if (source == "/") {
			target = strappend<Char>(target, &bufSize, "C:", 2);
			return target - buf;
		}
	}

	if (source.starts_with(Char('/'))) {
		++source;
		auto sub = source.template readUntil<typename StringViewBase<Char>::template Chars<'/'>>();
		if (sub.size() == 1 && sub.template is<typename StringViewBase<Char>::Latin>()) {
			auto driveLetter = toupper_c(sub.at(0));
			auto c = Char(':');
			target = strappend<Char, false>(target, &bufSize, &driveLetter, 1);
			target = strappend<Char, false>(target, &bufSize, &c, 1);
			// C: - path to current dir on drive C,
			// but C:\ - path to the drive's root, so, add \ to the end of string
			if (source.empty()) {
				c = Char('\\');
				target = strappend<Char, false>(target, &bufSize, &c, 1);
			}
		} else {
			auto c = Char('\\');
			target = strappend<Char, false>(target, &bufSize, &c, 1);
			target = strappend<Char, false>(target, &bufSize, sub.data(), sub.size());
		}
	}

	while (!source.empty()) {
		if (source.is(Char('/'))) {
			auto c = Char('\\');
			target = strappend<Char, false>(target, &bufSize, &c, 1);
			source.template skipChars<typename StringViewBase<Char>::template Chars<'/'>>();
		}
		auto component =
				source.template readUntil<typename StringViewBase<Char>::template Chars<'/'>>();
		if (!component.empty()) {
			target = strappend<Char, false>(target, &bufSize, component.data(), component.size());
		}
	}
	if (bufSize > 0) {
		target[0] = 0;
	}
	return target - buf;
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(fpath_to_posix)(const char *path,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize) {
	return fpath_to_posix(path, pathSize, buf, bufSize);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(fpath_to_native)(const char *path,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize) {
	return fpath_to_native(path, pathSize, buf, bufSize);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wfpath_to_posix)(const __SPRT_ID(wchar_t) * path,
		__SPRT_ID(size_t) pathSize, __SPRT_ID(wchar_t) * buf, __SPRT_ID(size_t) bufSize) {
	return fpath_to_posix((const char16_t *)path, pathSize, (char16_t *)buf, bufSize);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wfpath_to_native)(const __SPRT_ID(wchar_t) * path,
		__SPRT_ID(size_t) pathSize, __SPRT_ID(wchar_t) * buf, __SPRT_ID(size_t) bufSize) {
	return fpath_to_native((const char16_t *)path, pathSize, (char16_t *)buf, bufSize);
}

__SPRT_C_FUNC int remove(const char *path) __SPRT_NOEXCEPT {
	return platform::performWithNativePath(path, [&](const char *nativePath) {
		// call with native path
		auto wNativePath = __MALLOCA_WSTRING(nativePath);
		DWORD attributes = GetFileAttributesW(wNativePath);
		if (attributes == INVALID_FILE_ATTRIBUTES) {
			__sprt_freea(wNativePath);
			__sprt_errno = ENOENT;
			return -1;
		}
		if ((attributes & FILE_ATTRIBUTE_DIRECTORY)) {
			if (RemoveDirectoryW(wNativePath)) {
				__sprt_freea(wNativePath);
				return 0;
			}
		} else {
			if (DeleteFileW(wNativePath)) {
				__sprt_freea(wNativePath);
				return 0;
			}
		}
		__sprt_freea(wNativePath);
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}, -1);
}

static int __rename(const char *oldPath, const char *newPath) {
	auto wOldPath = __MALLOCA_WSTRING(oldPath);
	auto wNewPath = __MALLOCA_WSTRING(newPath);

	int result = 0;
	BOOL success =
			MoveFileExW(wOldPath, wNewPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);

	if (!success) {
		DWORD error = GetLastError();
		// Map Windows errors to POSIX errno values
		switch (error) {
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND: __sprt_errno = ENOENT; break;
		case ERROR_ACCESS_DENIED: __sprt_errno = EACCES; break;
		case ERROR_ALREADY_EXISTS: __sprt_errno = EEXIST; break;
		case ERROR_SHARING_VIOLATION: __sprt_errno = EBUSY; break;
		case ERROR_INVALID_NAME: __sprt_errno = EINVAL; break;
		default: __sprt_errno = EIO; break;
		}
		result = -1;
	}

	__sprt_freea(wOldPath);
	__sprt_freea(wNewPath);
	return result;
}

__SPRT_C_FUNC int rename(const char *oldPath, const char *newPath) __SPRT_NOEXCEPT {
	return platform::performWithNativePath(oldPath, [&](const char *oldTarget) SPRT_LAMBDAINLINE {
		return platform::performWithNativePath(newPath,
				[&](const char *newTarget) SPRT_LAMBDAINLINE {
			// call with native path
			return __rename(oldTarget, newTarget);
		}, -1);
	}, -1);
}

__SPRT_C_FUNC int renameat(int oldfd, const char *oldPath, int newfd,
		const char *newPath) __SPRT_NOEXCEPT {
	int ret = -1;
	platform::openAtPath(oldfd, oldPath, [&](const char *oldTarget, size_t) {
		platform::openAtPath(newfd, newPath, [&](const char *newTarget, size_t) {
			ret = rename(oldTarget, newTarget); //
		});
	});
	return ret;
}

bool __mktmppath(char *__itpl, size_t suffixLen, const Callback<bool(const char *, size_t)> &cb);

thread_local char tl_tmpnam_buf[L_tmpnam + 1] = {0};

struct _TmpPathStorage {
	static constexpr size_t wPathBufferLen = MAX_PATH + 2;
	wchar_t wtmpDirPath[wPathBufferLen] = {0};
	char tmpDirPath[wPathBufferLen * 3] = {0};
	size_t dirLength = 0;

	_TmpPathStorage() {
		auto wdirLength = GetTempPathW(wPathBufferLen, wtmpDirPath);
		if (wdirLength > 0) {
			unicode::toUtf8(tmpDirPath, wPathBufferLen * 3,
					WideStringView((char16_t *)wtmpDirPath, wdirLength), &dirLength);
		}
	}
};

static _TmpPathStorage s_tmpDirStorage;

__SPRT_C_FUNC char *tmpnam(char *s) __SPRT_NOEXCEPT {
	if (s_tmpDirStorage.dirLength == 0) {
		errno = ENOSPC;
		return nullptr;
	}

	if (!s) {
		s = tl_tmpnam_buf;
	}

	auto target = s;
	size_t bufLen = L_tmpnam;

	target = strappend(target, &bufLen, s_tmpDirStorage.tmpDirPath, s_tmpDirStorage.dirLength);
	target = strappend(target, &bufLen, "tmpnam_XXXXXX", "tmpnam_XXXXXX"_len);

	if (bufLen > 0) {
		if (__mktmppath(s, 0, [](const char *path, size_t) {
			return ::access(path, F_OK) == 0; //
		})) {
			return s;
		}
		return nullptr;
	}

	errno = ENOBUFS;
	return nullptr;
}

__SPRT_C_FUNC char *tmpnam_r(char *s) __SPRT_NOEXCEPT {
	if (!s) {
		return nullptr;
	}

	return tmpnam(s);
}

__SPRT_C_FUNC FILE *tmpfile(void) __SPRT_NOEXCEPT {
	if (s_tmpDirStorage.dirLength == 0) {
		errno = ENOSPC;
		return nullptr;
	}

	char buf[L_tmpnam + 1] = {0};

	auto target = buf;
	size_t bufLen = L_tmpnam;

	target = strappend(target, &bufLen, s_tmpDirStorage.tmpDirPath, s_tmpDirStorage.dirLength);
	target = strappend(target, &bufLen, "tmpnam_XXXXXX", "tmpnam_XXXXXX"_len);

	auto fd = mkstemp(target);
	if (!fd) {
		return nullptr;
	}

	auto f = fdopen(fd, "w+");
	if (!f) {
		close(fd);
		return nullptr;
	}

	return f;
}

} // namespace sprt
