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

#include <stdarg.h>
#include <stdio.h>
#include <locale.h>

#if SPRT_LINUX || SPRT_ANDROID
#include <locale.h>
#endif

#if SPRT_MACOS
#include <xlocale.h>
#endif

namespace sprt {

__SPRT_C_FUNC __SPRT_ID(FILE)
		* __SPRT_ID(
				fopen_impl)(const char *__SPRT_RESTRICT path, const char *__SPRT_RESTRICT mode) {
#if SPRT_MACOS
	return ::fopen(path, mode);
#else
	return ::fopen64(path, mode);
#endif
}

__SPRT_C_FUNC __SPRT_ID(FILE)
		* __SPRT_ID(freopen_impl)(const char *__SPRT_RESTRICT path,
				const char *__SPRT_RESTRICT mode, __SPRT_ID(FILE) * __SPRT_RESTRICT file) {
#if SPRT_MACOS
	return ::freopen(path, mode, file);
#else
	return ::freopen64(path, mode, file);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fclose_impl)(__SPRT_ID(FILE) * file) { return ::fclose(file); }

__SPRT_C_FUNC int __SPRT_ID(remove_impl)(const char *path) { return remove(path); }

__SPRT_C_FUNC int __SPRT_ID(rename_impl)(const char *oldPath, const char *newPath) {
	return rename(oldPath, newPath);
}

__SPRT_C_FUNC int __SPRT_ID(feof_impl)(__SPRT_ID(FILE) * file) { return ::feof(file); }

__SPRT_C_FUNC int __SPRT_ID(ferror_impl)(__SPRT_ID(FILE) * file) { return ::ferror(file); }

__SPRT_C_FUNC void __SPRT_ID(clearerr_impl)(__SPRT_ID(FILE) * file) { return ::clearerr(file); }

__SPRT_C_FUNC int __SPRT_ID(fseek_impl)(__SPRT_ID(FILE) * file, long pos, int when) {
	return ::fseek(file, pos, when);
}

__SPRT_C_FUNC long __SPRT_ID(ftell_impl)(__SPRT_ID(FILE) * file) { return ::ftell(file); }

__SPRT_C_FUNC void __SPRT_ID(rewind_impl)(__SPRT_ID(FILE) * file) { return ::rewind(file); }

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
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

	auto ret = ::vprintf(fmt, list);

	__builtin_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(fprintf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT file,
		const char *__SPRT_RESTRICT fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

	auto ret = ::vfprintf(file, fmt, list);

	__builtin_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		sprintf_impl)(char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#ifdef SPRT_WINDOWS
	auto ret = ::vsprintf(buf, fmt, list);
#else
	auto ret = ::vsprintf(buf, fmt, list);
#endif
#pragma clang diagnostic pop

	__builtin_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		snprintf_impl)(char *__SPRT_RESTRICT buf, size_t n, const char *__SPRT_RESTRICT fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

	auto ret = ::vsnprintf(buf, n, fmt, list);

	__builtin_va_end(list);
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#ifdef SPRT_WINDOWS
	return ::vsprintf(buf, fmt, arg);
#else
	return ::vsprintf(buf, fmt, arg);
#endif
#pragma clang diagnostic pop
}

__SPRT_C_FUNC int __SPRT_ID(vsnprintf_impl)(char *__SPRT_RESTRICT buf, size_t n,
		const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) arg) {
	return ::vsnprintf(buf, n, fmt, arg);
}


__SPRT_C_FUNC int __SPRT_ID(scanf_impl)(const char *__SPRT_RESTRICT fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

	auto ret = ::vscanf(fmt, list);

	__builtin_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		fscanf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

	auto ret = ::vfscanf(file, fmt, list);

	__builtin_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		sscanf_impl)(const char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

	auto ret = ::vsscanf(buf, fmt, list);

	__builtin_va_end(list);
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

__SPRT_C_FUNC int __SPRT_ID(setvbuf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT file,
		char *__SPRT_RESTRICT buf, int mode, size_t size) {
	return ::setvbuf(file, buf, mode, size);
}

__SPRT_C_FUNC void __SPRT_ID(
		setbuf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT file, char *__SPRT_RESTRICT buf) {
	::setbuf(file, buf);
}

__SPRT_C_FUNC char *__SPRT_ID(tmpnam_impl)(char *buf) { return ::tmpnam(buf); }

__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(tmpfile_impl)(void) { return ::tmpfile(); }

__SPRT_C_FUNC int __SPRT_ID(asprintf)(char **out, const char *fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

	auto ret = ::vasprintf(out, fmt, list);

	__builtin_va_end(list);
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
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

	auto ret = ::vdprintf(n, fmt, list);

	__builtin_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		vdprintf)(int n, const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) list) {
	return ::vdprintf(n, fmt, list);
}

__SPRT_C_FUNC void __SPRT_ID(flockfile)(__SPRT_ID(FILE) * f) { ::flockfile(f); }

__SPRT_C_FUNC int __SPRT_ID(ftrylockfile)(__SPRT_ID(FILE) * f) { return ::ftrylockfile(f); }

__SPRT_C_FUNC void __SPRT_ID(funlockfile)(__SPRT_ID(FILE) * f) { ::funlockfile(f); }
__SPRT_C_FUNC int __SPRT_ID(getc_unlocked)(__SPRT_ID(FILE) * f) { return getc_unlocked(f); }

__SPRT_C_FUNC int __SPRT_ID(getchar_unlocked)(void) { return getchar_unlocked(); }

__SPRT_C_FUNC int __SPRT_ID(putc_unlocked)(int c, __SPRT_ID(FILE) * f) {
	return ::putc_unlocked(c, f);
}

__SPRT_C_FUNC int __SPRT_ID(putchar_unlocked)(int c) { return ::putchar_unlocked(c); }

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
	return ::renameat(oldfd, oldPath, newfd, newPath);
}

__SPRT_C_FUNC char *__SPRT_ID(ctermid)(char *s) {
#if SPRT_ANDROID
	if (platform::_ctermid) {
		return platform::_ctermid(s);
	}
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#elif SPRT_WINDOWS
	if (s) {
		*s = '\0';
	}
	return s;
#else
	return ::ctermid(s);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		scanf_l)(__SPRT_ID(locale_t) loc, const char *__SPRT_RESTRICT fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

#if SPRT_LINUX || SPRT_ANDROID || SPRT_WINDOWS
	auto cur = ::uselocale(loc);
	auto ret = ::vscanf(fmt, list);
	::uselocale(cur);
#else
	auto ret = ::vscanf_l(loc, fmt, list);
#endif

	__builtin_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(fscanf_l)(__SPRT_ID(FILE) * __SPRT_RESTRICT stream,
		__SPRT_ID(locale_t) loc, const char *__SPRT_RESTRICT fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

#if SPRT_LINUX || SPRT_ANDROID || SPRT_WINDOWS
	auto cur = ::uselocale(loc);
	auto ret = ::vfscanf(stream, fmt, list);
	::uselocale(cur);
#else
	auto ret = ::fscanf_l(stream, loc, fmt, list);
#endif

	__builtin_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(sscanf_l)(const char *__SPRT_RESTRICT buf, __SPRT_ID(locale_t) loc,
		const char *__SPRT_RESTRICT fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

#if SPRT_LINUX || SPRT_ANDROID || SPRT_WINDOWS
	auto cur = ::uselocale(loc);
	auto ret = ::vsscanf(buf, fmt, list);
	::uselocale(cur);
#else
	auto ret = ::vsscanf_l(buf, loc, fmt, list);
#endif

	__builtin_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(vscanf_l)(__SPRT_ID(locale_t) loc, const char *__SPRT_RESTRICT format,
		__SPRT_ID(va_list) ap) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_WINDOWS
	auto cur = ::uselocale(loc);
	auto ret = ::vscanf(format, ap);
	::uselocale(cur);
	return ret;
#else
	return vscanf_l(loc, format, ap);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(vfscanf_l)(__SPRT_ID(FILE) * __SPRT_RESTRICT stream,
		__SPRT_ID(locale_t) loc, const char *__SPRT_RESTRICT format, __SPRT_ID(va_list) ap) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_WINDOWS
	auto cur = ::uselocale(loc);
	auto ret = ::vfscanf(stream, format, ap);
	::uselocale(cur);
	return ret;
#else
	return vfscanf_l(stream, loc, format, ap);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(vsscanf_l)(const char *__SPRT_RESTRICT str, __SPRT_ID(locale_t) loc,
		const char *__SPRT_RESTRICT format, __SPRT_ID(va_list) ap) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_WINDOWS
	auto cur = ::uselocale(loc);
	auto ret = ::vsscanf(str, format, ap);
	::uselocale(cur);
	return ret;
#else
	return vsscanf_l(str, loc, format, ap);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(snprintf_l)(char *__SPRT_RESTRICT buf, __SPRT_ID(size_t) len,
		__SPRT_ID(locale_t) __SPRT_RESTRICT loc, const char *__SPRT_RESTRICT fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

#if SPRT_LINUX || SPRT_ANDROID || SPRT_WINDOWS
	auto cur = ::uselocale(loc);
	auto ret = ::vsnprintf(buf, len, fmt, list);
	::uselocale(cur);
#else
	auto ret = vsnprintf_l(buf, len, loc, fmt, list);
#endif

	__builtin_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(vsnprintf_l)(char *__SPRT_RESTRICT buf, __SPRT_ID(size_t) len,
		__SPRT_ID(locale_t) __SPRT_RESTRICT loc, const char *__SPRT_RESTRICT fmt,
		__SPRT_ID(va_list) list) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_WINDOWS
	auto cur = ::uselocale(loc);
	auto ret = ::vsnprintf(buf, len, fmt, list);
	::uselocale(cur);
	return ret;
#else
	return vsnprintf_l(buf, len, loc, fmt, list);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(asprintf_l)(char **__SPRT_RESTRICT target,
		__SPRT_ID(locale_t) __SPRT_RESTRICT loc, const char *__SPRT_RESTRICT fmt, ...) {
	__builtin_va_list list;
	__builtin_va_start(list, fmt);

#if SPRT_LINUX || SPRT_ANDROID || SPRT_WINDOWS
	auto cur = ::uselocale(loc);
	auto ret = ::vasprintf(target, fmt, list);
	::uselocale(cur);
#else
	auto ret = vasprintf_l(target, loc, fmt, list);
#endif

	__builtin_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(vasprintf_l)(char **__SPRT_RESTRICT target,
		__SPRT_ID(locale_t) __SPRT_RESTRICT loc, const char *__SPRT_RESTRICT fmt,
		__SPRT_ID(va_list) list) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_WINDOWS
	auto cur = ::uselocale(loc);
	auto ret = ::vasprintf(target, fmt, list);
	::uselocale(cur);
	return ret;
#else
	return vasprintf_l(target, loc, fmt, list);
#endif
}

#if !SPRT_WINDOWS
__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(fpath_to_posix)(const char *path,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize) {
	if (bufSize < pathSize) {
		return 0;
	}

	__sprt_memcpy(buf, path, pathSize);
	if (bufSize > pathSize) {
		buf[pathSize] = 0; // optional nullterm
	}
	return pathSize;
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(fpath_to_native)(const char *path,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize) {
	if (bufSize < pathSize) {
		return 0;
	}

	__sprt_memmove(buf, path, pathSize);
	if (bufSize > pathSize) {
		buf[pathSize] = 0; // optional nullterm
	}
	return pathSize;
}
#endif

} // namespace sprt
