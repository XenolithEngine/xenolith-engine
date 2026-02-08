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

#include <sprt/runtime/string.h>
#include <sprt/runtime/stringview.h>
#include <sprt/c/__sprt_errno.h>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#include <Windows.h>
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>
#include <io.h>
#include <stdio.h>

#include "private/SPRTFilename.h"
#include "private/SPRTSpecific.h"

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

static __SPRT_ID(size_t) __fpath_to_posix(const char *path, __SPRT_ID(size_t) pathSize, char *buf,
		__SPRT_ID(size_t) bufSize) {
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

static __SPRT_ID(size_t) __fpath_to_native(const char *path, __SPRT_ID(size_t) pathSize, char *buf,
		__SPRT_ID(size_t) bufSize) {
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

static __SPRT_ID(FILE)
		* fmemopen(void *__SPRT_RESTRICT __ptr, __SPRT_ID(size_t) __size,
				const char *__SPRT_RESTRICT mode) {
	static constexpr size_t PathBufferLen = MAX_PATH + 2;
	static constexpr size_t PrefixBufferLen = 24;

	wchar_t tmpDirPath[PathBufferLen] = {0};
	wchar_t tmpFilePath[PathBufferLen] = {0};
	wchar_t tmpPrefixPath[PrefixBufferLen] = {0};

	auto dirLength = GetTempPathW(PathBufferLen, tmpDirPath);
	if (dirLength < 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return nullptr;
	}

	size_t tmpPrefixPathBufLen = PrefixBufferLen;
	auto target = strappend(tmpPrefixPath, &tmpPrefixPathBufLen, L"sfmop", 5);
	target = strappend(target, &tmpPrefixPathBufLen, GetCurrentProcessId());

	if (GetTempFileNameW(tmpDirPath, tmpPrefixPath, 0, tmpFilePath) == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return nullptr;
	}

	int fd = -1;
	if (_wsopen_s(&fd, tmpFilePath,
				_O_CREAT | _O_SHORT_LIVED | _O_TEMPORARY | _O_RDWR | _O_BINARY | _O_NOINHERIT,
				_SH_DENYRW, _S_IREAD | _S_IWRITE)
			!= 0) {
		return nullptr;
	}

	FILE *ret = nullptr;
	internal::performBinaryMode(mode, [&](const char *modeBuf) { ret = _fdopen(fd, modeBuf); });

	if (!ret) {
		_close(fd);
		return nullptr;
	}

	if (__size > 0) {
		if (__ptr) {
			fwrite(__ptr, __size, 1, ret);
		} else {
			auto ptr = __sprt_malloca(__size);
			fwrite(ptr, __size, 1, ret);
			__sprt_freea(ptr);
		}
	}

	rewind(ret);

	return ret;
}

ssize_t getdelim(char **__SPRT_RESTRICT __lineptr, size_t *__SPRT_RESTRICT __n, int __delim,
		FILE *__SPRT_RESTRICT __stream) {
	if (__lineptr == NULL || __n == NULL || __stream == NULL || (*__lineptr == NULL && *__n != 0)) {
		errno = EINVAL;
		return -1;
	}
	if (feof(__stream) || ferror(__stream)) {
		return -1;
	}

	if (*__lineptr == NULL) {
		*__n = 256;
		*__lineptr = (char *)malloc(*__n);
		if (*__lineptr == NULL) {
			//fseterr(__stream);
			errno = ENOMEM;
			return -1;
		}
	}
	ssize_t nread = 0;
	int c = EOF;
	while (c != __delim) {
		c = fgetc(__stream);
		if (c == EOF) {
			break;
		}
		if (nread >= (ssize_t)(*__n - 1)) {
			size_t newn = *__n * 2;
			char *newptr = (char *)realloc(*__lineptr, newn);
			if (newptr == NULL) {
				//fseterr(stream);
				errno = ENOMEM;
				return -1;
			}
			*__lineptr = newptr;
			*__n = newn;
		}
		(*__lineptr)[nread++] = c;
	}
	if (c == EOF && nread == 0) {
		return -1;
	}
	(*__lineptr)[nread] = 0;
	return nread;
}

ssize_t getline(char **__SPRT_RESTRICT lineptr, size_t *__SPRT_RESTRICT n,
		FILE *__SPRT_RESTRICT stream) {
	return getdelim(lineptr, n, '\n', stream);
}

} // namespace sprt
