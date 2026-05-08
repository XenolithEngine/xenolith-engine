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
#include "stdio.h"

#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>

namespace sprt {

static int __is_exact_windows_native(StringView source) {
	// Any path, that starts with \ is windows-native
	if (source.starts_with('\\')) {
		return 1;
	}

	// C:Directory, C:\Directory, C:/Directory is Windows native
	if (source.size() > 1 && source.is<StringView::LatinUppercase>() && source.at(1) == ':') {
		return 1;
	}

	// Any path with \ in it is windows native
	source.readUntil<StringView::Chars<'\\'>>();
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
		// Actually, we can pass paths with / to WinAPI, but we can not elsewhere detect other special convertion cases
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

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(fpath_to_posix)(const char *path,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize) {
	if (bufSize < pathSize) {
		return 0;
	}

	auto target = buf;
	StringView source(path, pathSize);
	if (source.starts_with("\\\\?\\")) {
		// Windows 10 version 1803 is required for the runtime, we can support long paths without
		// DOS device prefix, just skip it
		source += __constexpr_strlen("\\\\?\\");
	}
	if (source.size() > 2 && source.is<StringView::Latin>() && source.at(1) == ':'
			&& source.at(2) == '\\') {
		auto driveLetter = tolower_c(source.at(0));
		source += 2;
		target = strappend<char, false>(target, &bufSize, "/", 1);
		target = strappend<char, false>(target, &bufSize, &driveLetter, 1);
	} else if (source.size() > 1 && source.is<StringView::Latin>() && source.at(1) == ':') {
		auto driveLetter = tolower_c(source.at(0));
		source += 2;
		target = strappend<char, false>(target, &bufSize, "/", 1);
		target = strappend<char, false>(target, &bufSize, &driveLetter, 1);
	}

	while (!source.empty()) {
		if (source.is('\\')) {
			source.skipChars<StringView::Chars<'\\'>>();
			target = strappend<char, false>(target, &bufSize, "/", 1);
		}
		auto component = source.readUntil<StringView::Chars<'\\'>>();
		if (!component.empty()) {
			target = strappend<char, false>(target, &bufSize, component.data(), component.size());
		}
	}
	if (bufSize > 0) {
		target[0] = 0;
	}
	return target - buf;
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(fpath_to_native)(const char *path,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize) {
	if (bufSize < pathSize) {
		return 0;
	}

	auto target = buf;
	StringView source(path, pathSize);
	if (source == "/") {
		target = strappend<char>(target, &bufSize, "C:", 2);
		return target - buf;
	}

	if (source.starts_with('/')) {
		++source;
		auto sub = source.readUntil<StringView::Chars<'/'>>();
		if (sub.size() == 1 && sub.is<StringView::Latin>()) {
			auto driveLetter = toupper_c(sub.at(0));
			target = strappend<char, false>(target, &bufSize, &driveLetter, 1);
			target = strappend<char, false>(target, &bufSize, ":", 1);
			// C: - path to current dir on drive C,
			// but C:\ - path to the drive's root, so, add \ to the end of string
			if (source.empty()) {
				target = strappend<char, false>(target, &bufSize, "\\", 1);
			}
		} else {
			target = strappend<char, false>(target, &bufSize, "\\", 1);
			target = strappend<char, false>(target, &bufSize, sub.data(), sub.size());
		}
	}

	while (!source.empty()) {
		if (source.is('/')) {
			target = strappend<char, false>(target, &bufSize, "\\", 1);
			source.skipChars<StringView::Chars<'/'>>();
		}
		auto component = source.readUntil<StringView::Chars<'/'>>();
		if (!component.empty()) {
			target = strappend<char, false>(target, &bufSize, component.data(), component.size());
		}
	}
	if (bufSize > 0) {
		target[0] = 0;
	}
	return target - buf;
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

} // namespace sprt
