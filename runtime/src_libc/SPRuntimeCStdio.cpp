/**
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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_errno.h>

#include <sprt/runtime/log.h>
#include "private/SPRTFilename.h"
#include "private/SPRTSpecific.h"

#include <stdarg.h>
#include <stdio.h>

#if SPRT_WINDOWS
#include "platform/windows/stdio.cc"
#endif

namespace sprt {

__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(stdin_impl)() {
	return stdin; //
}
__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(stdout_impl)() {
	return stdout; //
}
__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(stderr_impl)() {
	return stderr; //
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(fpath_to_posix)(const char *path,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize) {
	if (bufSize < pathSize) {
		return 0;
	}

#if SPRT_WINDOWS
	return __fpath_to_posix(path, pathSize, buf, bufSize);
#else
	__sprt_memcpy(buf, path, pathSize);
	if (bufSize > pathSize) {
		buf[pathSize] = 0; // optional nullterm
	}
	return pathSize;
#endif
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(fpath_to_native)(const char *path,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize) {
	if (bufSize < pathSize) {
		return 0;
	}

#if SPRT_WINDOWS
	return __fpath_to_native(path, pathSize, buf, bufSize);
#else
	__sprt_memmove(buf, path, pathSize);
	if (bufSize > pathSize) {
		buf[pathSize] = 0; // optional nullterm
	}
	return pathSize;
#endif
}

__SPRT_C_FUNC __SPRT_ID(FILE)
		* __SPRT_ID(
				fopen_impl)(const char *__SPRT_RESTRICT path, const char *__SPRT_RESTRICT mode) {
#if SPRT_WINDOWS
	FILE *ret = nullptr;
	internal::performWithNativePath(path, [&](const char *nativePath) SPRT_LAMBDAINLINE {
		internal::performBinaryMode(mode, [&](const char *modeBuf) SPRT_LAMBDAINLINE {
			__sprt_errno = fopen_s(&ret, nativePath, modeBuf);
		});
		return 0;
	}, 0);
	return ret;
#else
	return ::fopen64(path, mode);
#endif
}

__SPRT_C_FUNC __SPRT_ID(FILE)
		* __SPRT_ID(freopen_impl)(const char *__SPRT_RESTRICT path,
				const char *__SPRT_RESTRICT mode, __SPRT_ID(FILE) * __SPRT_RESTRICT file) {
#if SPRT_WINDOWS
	FILE *ret = nullptr;
	internal::performWithNativePath(path, [&](const char *nativePath) SPRT_LAMBDAINLINE {
		internal::performBinaryMode(mode, [&](const char *modeBuf) SPRT_LAMBDAINLINE {
			__sprt_errno = freopen_s(&ret, nativePath, modeBuf, file);
		});
		return 0;
	}, 0);
	return ret;
#else
	return ::freopen64(path, mode, file);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fclose_impl)(__SPRT_ID(FILE) * file) { return ::fclose(file); }

__SPRT_C_FUNC int __SPRT_ID(remove_impl)(const char *path) {
#if SPRT_WINDOWS
	return internal::performWithNativePath(path, [&](const char *nativePath) {
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
#else
	return ::remove(path);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(rename_impl)(const char *oldPath, const char *newPath) {
#if SPRT_WINDOWS
	return internal::performWithNativePath(oldPath, [&](const char *oldTarget) SPRT_LAMBDAINLINE {
		return internal::performWithNativePath(newPath,
				[&](const char *newTarget) SPRT_LAMBDAINLINE {
			// call with native path
			return ::rename(oldTarget, newTarget);
		}, -1);
	}, -1);
#else
	return ::rename(oldPath, newPath);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(feof_impl)(__SPRT_ID(FILE) * file) { return ::feof(file); }

__SPRT_C_FUNC int __SPRT_ID(ferror_impl)(__SPRT_ID(FILE) * file) { return ::ferror(file); }

__SPRT_C_FUNC int __SPRT_ID(fflush_impl)(__SPRT_ID(FILE) * file) { return ::fflush(file); }

__SPRT_C_FUNC void __SPRT_ID(clearerr_impl)(__SPRT_ID(FILE) * file) { return ::clearerr(file); }

__SPRT_C_FUNC int __SPRT_ID(fseek_impl)(__SPRT_ID(FILE) * file, long pos, int when) {
	return ::fseek(file, pos, when);
}

__SPRT_C_FUNC long __SPRT_ID(ftell_impl)(__SPRT_ID(FILE) * file) { return ::ftell(file); }

__SPRT_C_FUNC void __SPRT_ID(rewind_impl)(__SPRT_ID(FILE) * file) { return ::rewind(file); }

__SPRT_C_FUNC size_t __SPRT_ID(fread_impl)(void *__SPRT_RESTRICT buf, size_t n, size_t count,
		__SPRT_ID(FILE) * __SPRT_RESTRICT file) {
	return ::fread(buf, n, count, file);
}

__SPRT_C_FUNC size_t __SPRT_ID(fwrite_impl)(const void *__SPRT_RESTRICT buf, size_t n, size_t count,
		__SPRT_ID(FILE) * __SPRT_RESTRICT file) {
	return ::fwrite(buf, n, count, file);
}

__SPRT_C_FUNC int __SPRT_ID(fgetc_impl)(__SPRT_ID(FILE) * file) { return ::fgetc(file); }

__SPRT_C_FUNC int __SPRT_ID(getc_impl)(__SPRT_ID(FILE) * file) { return ::getc(file); }

__SPRT_C_FUNC int __SPRT_ID(getchar_impl)(void) { return ::getchar(); }

__SPRT_C_FUNC int __SPRT_ID(ungetc_impl)(int c, __SPRT_ID(FILE) * file) {
	return ::ungetc(c, file);
}

__SPRT_C_FUNC int __SPRT_ID(fputc_impl)(int c, __SPRT_ID(FILE) * file) { return ::fputc(c, file); }

__SPRT_C_FUNC int __SPRT_ID(putc_impl)(int c, __SPRT_ID(FILE) * file) { return ::putc(c, file); }

__SPRT_C_FUNC int __SPRT_ID(putchar_impl)(int c) { return ::putchar(c); }

__SPRT_C_FUNC char *__SPRT_ID(
		fgets_impl)(char *__SPRT_RESTRICT buf, int n, __SPRT_ID(FILE) * __SPRT_RESTRICT file) {
	return ::fgets(buf, n, file);
}

__SPRT_C_FUNC int __SPRT_ID(
		fputs_impl)(const char *__SPRT_RESTRICT buf, __SPRT_ID(FILE) * __SPRT_RESTRICT file) {
	return ::fputs(buf, file);
}

__SPRT_C_FUNC int __SPRT_ID(puts_impl)(const char *str) { return ::puts(str); }

__SPRT_C_FUNC int __SPRT_ID(printf_impl)(const char *__SPRT_RESTRICT fmt, ...) {
	va_list list;
	va_start(list, fmt);

	auto ret = ::vprintf(fmt, list);

	va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(fprintf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT file,
		const char *__SPRT_RESTRICT fmt, ...) {
	va_list list;
	va_start(list, fmt);

	auto ret = ::vfprintf(file, fmt, list);

	va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		sprintf_impl)(char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt, ...) {
	va_list list;
	va_start(list, fmt);


#ifdef SPRT_WINDOWS
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	auto ret = ::vsprintf(buf, fmt, list);
#pragma clang diagnostic pop
#else
	auto ret = ::vsprintf(buf, fmt, list);
#endif


	va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		snprintf_impl)(char *__SPRT_RESTRICT buf, size_t n, const char *__SPRT_RESTRICT fmt, ...) {
	va_list list;
	va_start(list, fmt);

	auto ret = ::vsnprintf(buf, n, fmt, list);

	va_end(list);
	return ret;
}


__SPRT_C_FUNC int __SPRT_ID(vprintf_impl)(const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) arg) {
	return ::vprintf(fmt, arg);
}

__SPRT_C_FUNC int __SPRT_ID(vfprintf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT file,
		const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) arg) {
	return ::vfprintf(file, fmt, arg);
}

__SPRT_C_FUNC int __SPRT_ID(vsprintf_impl)(char *__SPRT_RESTRICT buf,
		const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) arg) {
#ifdef SPRT_WINDOWS
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	return ::vsprintf(buf, fmt, arg);
#pragma clang diagnostic pop
#else
	return ::vsprintf(buf, fmt, arg);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(vsnprintf_impl)(char *__SPRT_RESTRICT buf, size_t n,
		const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) arg) {
	return ::vsnprintf(buf, n, fmt, arg);
}


__SPRT_C_FUNC int __SPRT_ID(scanf_impl)(const char *__SPRT_RESTRICT fmt, ...) {
	va_list list;
	va_start(list, fmt);

	auto ret = ::vscanf(fmt, list);

	va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		fscanf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt, ...) {
	va_list list;
	va_start(list, fmt);

	auto ret = ::vfscanf(file, fmt, list);

	va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		sscanf_impl)(const char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt, ...) {
	va_list list;
	va_start(list, fmt);

	auto ret = ::vsscanf(buf, fmt, list);

	va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(vscanf_impl)(const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) arg) {
	return ::vscanf(fmt, arg);
}

__SPRT_C_FUNC int __SPRT_ID(vfscanf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT file,
		const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) arg) {
	return ::vfscanf(file, fmt, arg);
}

__SPRT_C_FUNC int __SPRT_ID(vsscanf_impl)(const char *__SPRT_RESTRICT buf,
		const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) arg) {
	return ::vsscanf(buf, fmt, arg);
}


__SPRT_C_FUNC void __SPRT_ID(perror_impl)(const char *err) { return ::perror(err); }


__SPRT_C_FUNC int __SPRT_ID(setvbuf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT file,
		char *__SPRT_RESTRICT buf, int mode, size_t size) {
	return ::setvbuf(file, buf, mode, size);
}

__SPRT_C_FUNC void __SPRT_ID(
		setbuf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT file, char *__SPRT_RESTRICT buf) {
#ifdef SPRT_WINDOWS
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	::setbuf(file, buf);
#pragma clang diagnostic pop
#else
	::setbuf(file, buf);
#endif
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

__SPRT_C_FUNC char *__SPRT_ID(tmpnam_impl)(char *buf) { return ::tmpnam(buf); }

#ifdef __clang__
#pragma clang diagnostic pop
#endif

__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(tmpfile_impl)(void) {

#ifdef SPRT_WINDOWS
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	return ::tmpfile();
#pragma clang diagnostic pop
#else
	return ::tmpfile();
#endif
}

__SPRT_C_FUNC int __SPRT_ID(asprintf)(char **out, const char *fmt, ...) {
	va_list list;
	va_start(list, fmt);

#if SPRT_WINDOWS
	va_list tmpList;
	va_copy(tmpList, list);
	int l = vsnprintf(0, 0, fmt, tmpList);
	va_end(tmpList);

	if (l < 0 || !(*out = (char *)__sprt_malloc(l + 1U))) {
		return -1;
	}
	auto ret = vsnprintf(*out, l + 1U, fmt, list);
#else
	auto ret = ::vasprintf(out, fmt, list);
#endif

	va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(vasprintf)(char **out, const char *fmt, __SPRT_ID(va_list) list) {
#if SPRT_WINDOWS
	va_list tmpList;
	va_copy(tmpList, list);
	int l = vsnprintf(0, 0, fmt, tmpList);
	va_end(tmpList);

	if (l < 0 || !(*out = (char *)__sprt_malloc(l + 1U))) {
		return -1;
	}
	return vsnprintf(*out, l + 1U, fmt, list);
#else
	return ::vasprintf(out, fmt, list);
#endif
}

__SPRT_C_FUNC __SPRT_ID(FILE)
		* __SPRT_ID(fmemopen)(void *__SPRT_RESTRICT ptr, __SPRT_ID(size_t) size,
				const char *__SPRT_RESTRICT mode) {
	return fmemopen(ptr, size, mode);
}

__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(open_memstream)(char **ptr, __SPRT_ID(size_t) * sz) {
#if __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM
	return ::open_memstream(ptr, sz);
#else
	__sprt_errno = ENOSYS;
	return nullptr;
#endif
}

__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(fdopen)(int fd, const char *mode) {
#if SPRT_WINDOWS
	FILE *ret = nullptr;
	internal::performBinaryMode(mode, [&](const char *modeBuf) {
		ret = ::_fdopen(fd, modeBuf); //
	});
	return ret;
#else
	return ::fdopen(fd, mode);
#endif
}

__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(popen)(const char *str, const char *mode) {
#if SPRT_WINDOWS
	FILE *ret = nullptr;
	internal::performBinaryMode(mode, [&](const char *modeBuf) { ret = ::_popen(str, modeBuf); });
	return ret;
#else
	return ::popen(str, mode);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(pclose)(__SPRT_ID(FILE) * f) {
#if SPRT_WINDOWS
	return ::_pclose(f);
#else
	return ::pclose(f);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fileno)(__SPRT_ID(FILE) * f) {
#if SPRT_WINDOWS
	return ::_fileno(f);
#else
	return ::fileno(f);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fseeko)(__SPRT_ID(FILE) * f, __SPRT_ID(off_t) off, int n) {
#if SPRT_WINDOWS
	return ::_fseeki64(f, off, n);
#else
	return ::fseeko(f, off, n);
#endif
}

__SPRT_C_FUNC __SPRT_ID(off_t) __SPRT_ID(ftello)(__SPRT_ID(FILE) * f) {
#if SPRT_WINDOWS
	return ::_ftelli64(f);
#else
	return ::ftello(f);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(dprintf)(int n, const char *__SPRT_RESTRICT fmt, ...) {
	va_list list;
	va_start(list, fmt);

#if SPRT_WINDOWS
	FILE *fp = _fdopen(_dup(n), "wb"); // Duplicate to avoid close issues
	if (!fp) {
		return -1;
	}

	int ret = vfprintf(fp, fmt, list);

	fclose(fp);
#else
	auto ret = ::vdprintf(n, fmt, list);
#endif

	va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		vdprintf)(int n, const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) list) {
#if SPRT_WINDOWS
	FILE *fp = _fdopen(_dup(n), "wb"); // Duplicate to avoid close issues
	if (!fp) {
		return -1;
	}

	int ret = vfprintf(fp, fmt, list);

	fclose(fp);
	return ret;
#else
	return ::vdprintf(n, fmt, list);
#endif
}

__SPRT_C_FUNC void __SPRT_ID(flockfile)(__SPRT_ID(FILE) * f) {
#if SPRT_WINDOWS
	::_lock_file(f);
#else
	::flockfile(f);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(ftrylockfile)(__SPRT_ID(FILE) * f) {
#if SPRT_WINDOWS
	return -1;
#else
	return ::ftrylockfile(f);
#endif
}

__SPRT_C_FUNC void __SPRT_ID(funlockfile)(__SPRT_ID(FILE) * f) {
#if SPRT_WINDOWS
	::_unlock_file(f);
#else
	::funlockfile(f);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(getc_unlocked)(__SPRT_ID(FILE) * f) {
#if SPRT_WINDOWS
	return _fgetc_nolock(f);
#else
	return ::getc_unlocked(f);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(getchar_unlocked)(void) {
#if SPRT_WINDOWS
	return _getchar_nolock();
#else
	return ::getchar_unlocked();
#endif
}

__SPRT_C_FUNC int __SPRT_ID(putc_unlocked)(int c, __SPRT_ID(FILE) * f) {
#if SPRT_WINDOWS
	return _fputc_nolock(c, f);
#else
	return ::putc_unlocked(c, f);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(putchar_unlocked)(int c) {
#if SPRT_WINDOWS
	return ::_putchar_nolock(c);
#else
	return ::putchar_unlocked(c);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(getdelim)(char **__SPRT_RESTRICT ret,
		__SPRT_ID(size_t) * __SPRT_RESTRICT sz, int c, __SPRT_ID(FILE) * __SPRT_RESTRICT f) {
	return getdelim(ret, sz, c, f);
}
__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(getline)(char **__SPRT_RESTRICT ret,
		__SPRT_ID(size_t) * __SPRT_RESTRICT sz, __SPRT_ID(FILE) * __SPRT_RESTRICT f) {
	return getline(ret, sz, f);
}
__SPRT_C_FUNC int __SPRT_ID(
		renameat)(int oldfd, const char *oldPath, int newfd, const char *newPath) {
#if SPRT_WINDOWS
	int ret = -1;
	platform::openAtPath(oldfd, oldPath, [&](const char *oldTarget, size_t) {
		platform::openAtPath(newfd, newPath,
				[&](const char *newTarget, size_t) { ret = rename(oldTarget, newTarget); });
	});
	return ret;
#else
	return ::renameat(oldfd, oldPath, newfd, newPath);
#endif
}

__SPRT_C_FUNC char *__SPRT_ID(ctermid)(char *s) {
#if SPRT_ANDROID
	if (platform::_ctermid) {
		return platform::_ctermid(s);
	}
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#elif SPRT_WINDOWS
	if (s) {
		*s = '\n';
	}
	return s;
#else
	return ::ctermid(s);
#endif
}

} // namespace sprt
