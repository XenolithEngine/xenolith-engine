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

#include <c/__sprt_string.h>
#include <c/__sprt_stdio.h>
#include <c/__sprt_errno.h>

#include "SPRuntimeLog.h"
#include "private/SPRTFilename.h"

#include <stdarg.h>
#include <stdio.h>

namespace sprt {

__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(stdin_impl)() { return ::stdin; }
__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(stdout_impl)() { return ::stdout; }
__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(stderr_impl)() { return ::stderr; }

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(fpath_to_posix)(const char *__SPRT_RESTRICT path,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize) {
	if (bufSize < pathSize) {
		return 0;
	}

	__sprt_memcpy(buf, path, pathSize);
#if SPRT_WINDOWS
#endif
	if (bufSize > pathSize) {
		buf[pathSize] = 0; // optional nullterm
	}
	return pathSize;
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(fpath_to_native)(const char *__SPRT_RESTRICT path,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize) {
	if (bufSize < pathSize) {
		return 0;
	}

	__sprt_memcpy(buf, path, pathSize);
#if SPRT_WINDOWS
#endif
	if (bufSize > pathSize) {
		buf[pathSize] = 0; // optional nullterm
	}
	return pathSize;
}

__SPRT_C_FUNC __SPRT_ID(FILE)
		* __SPRT_ID(
				fopen_impl)(const char *__SPRT_RESTRICT path, const char *__SPRT_RESTRICT mode) {
	if (!path || !mode) {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::stdio",
				"path or mode is not defined");
		return nullptr;
	}

	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::fopen64(target, mode);
	}, (__SPRT_ID(FILE) *)nullptr);
}

__SPRT_C_FUNC __SPRT_ID(FILE)
		* __SPRT_ID(freopen_impl)(const char *__SPRT_RESTRICT path,
				const char *__SPRT_RESTRICT mode, __SPRT_ID(FILE) * __SPRT_RESTRICT file) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::freopen64(target, mode, file);
	}, (__SPRT_ID(FILE) *)nullptr);
}

__SPRT_C_FUNC int __SPRT_ID(fclose_impl)(__SPRT_ID(FILE) * file) { return ::fclose(file); }

__SPRT_C_FUNC int __SPRT_ID(remove_impl)(const char *path) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::remove(target);
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(rename_impl)(const char *oldPath, const char *newPath) {
	return internal::performWithNativePath(oldPath, [&](const char *oldTarget) {
		return internal::performWithNativePath(newPath, [&](const char *newTarget) {
			// call with native path
			return ::rename(oldTarget, newTarget);
		}, -1);
	}, -1);
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

	auto ret = ::vsprintf(buf, fmt, list);

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
	return ::vsprintf(buf, fmt, arg);
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
	::setbuf(file, buf);
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

__SPRT_C_FUNC char *__SPRT_ID(tmpnam_impl)(char *buf) { return ::tmpnam(buf); }

#ifdef __clang__
#pragma clang diagnostic pop
#endif

__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(tmpfile_impl)(void) { return ::tmpfile(); }

__SPRT_C_FUNC int __SPRT_ID(asprintf)(char **out, const char *fmt, ...) {
	va_list list;
	va_start(list, fmt);

	auto ret = ::vasprintf(out, fmt, list);

	va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(vasprintf)(char **out, const char *fmt, __SPRT_ID(va_list) list) {
	return ::vasprintf(out, fmt, list);
}

__SPRT_C_FUNC __SPRT_ID(FILE)
		* __SPRT_ID(fmemopen)(void *__SPRT_RESTRICT ptr, __SPRT_ID(size_t) size,
				const char *__SPRT_RESTRICT mode) {
	return ::fmemopen(ptr, size, mode);
}
__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(open_memstream)(char **ptr, __SPRT_ID(size_t) * sz) {
	return ::open_memstream(ptr, sz);
}
__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(fdopen)(int fd, const char *mode) {
	return ::fdopen(fd, mode);
}
__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(popen)(const char *str, const char *mode) {
	return ::popen(str, mode);
}
__SPRT_C_FUNC int __SPRT_ID(pclose)(__SPRT_ID(FILE) * f) { return ::pclose(f); }
__SPRT_C_FUNC int __SPRT_ID(fileno)(__SPRT_ID(FILE) * f) { return ::fileno(f); }
__SPRT_C_FUNC int __SPRT_ID(fseeko)(__SPRT_ID(FILE) * f, __SPRT_ID(off_t) off, int n) {
	return ::fseeko(f, off, n);
}
__SPRT_C_FUNC __SPRT_ID(off_t) __SPRT_ID(ftello)(__SPRT_ID(FILE) * f) { return ::ftello(f); }
__SPRT_C_FUNC int __SPRT_ID(dprintf)(int n, const char *__SPRT_RESTRICT fmt, ...) {
	va_list list;
	va_start(list, fmt);

	auto ret = ::vdprintf(n, fmt, list);

	va_end(list);
	return ret;
}
__SPRT_C_FUNC int __SPRT_ID(
		vdprintf)(int n, const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) list) {
	return ::vdprintf(n, fmt, list);
}
__SPRT_C_FUNC void __SPRT_ID(flockfile)(__SPRT_ID(FILE) * f) { return ::flockfile(f); }
__SPRT_C_FUNC int __SPRT_ID(ftrylockfile)(__SPRT_ID(FILE) * f) { return ::ftrylockfile(f); }
__SPRT_C_FUNC void __SPRT_ID(funlockfile)(__SPRT_ID(FILE) * f) { return ::funlockfile(f); }
__SPRT_C_FUNC int __SPRT_ID(getc_unlocked)(__SPRT_ID(FILE) * f) { return ::getc_unlocked(f); }
__SPRT_C_FUNC int __SPRT_ID(getchar_unlocked)(void) { return ::getchar_unlocked(); }
__SPRT_C_FUNC int __SPRT_ID(putc_unlocked)(int c, __SPRT_ID(FILE) * f) {
	return ::putc_unlocked(c, f);
}
__SPRT_C_FUNC int __SPRT_ID(putchar_unlocked)(int c) { return ::putchar_unlocked(c); }
__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(getdelim)(char **__SPRT_RESTRICT ret,
		__SPRT_ID(size_t) * __SPRT_RESTRICT sz, int c, __SPRT_ID(FILE) * __SPRT_RESTRICT f) {
	return ::getdelim(ret, sz, c, f);
}
__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(getline)(char **__SPRT_RESTRICT ret,
		__SPRT_ID(size_t) * __SPRT_RESTRICT sz, __SPRT_ID(FILE) * __SPRT_RESTRICT f) {
	return ::getline(ret, sz, f);
}
__SPRT_C_FUNC int __SPRT_ID(
		renameat)(int oldfd, const char *oldPath, int newfd, const char *newPath) {
	return internal::performWithNativePath(oldPath, [&](const char *oldTarget) {
		return internal::performWithNativePath(newPath, [&](const char *newTarget) {
			// call with native path
			return ::renameat(oldfd, oldTarget, newfd, newTarget);
		}, -1);
	}, -1);
}
__SPRT_C_FUNC char *__SPRT_ID(ctermid)(char *s) { return ::ctermid(s); }


} // namespace sprt
