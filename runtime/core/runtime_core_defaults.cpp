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

#include <sprt/c/bits/__sprt_cpuset_t.h>
#include <sprt/cxx/detail/constexpr.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_dlfcn.h>
#include <sprt/runtime/log.h>

#if SPRT_LINUX || SPRT_ANDROID
#include <sched.h>
#include <stdio.h>
#include <dlfcn.h>
#include "linux/clock_gettime.cc"

#elif SPRT_MACOS
#include <sched.h>
#include <stdio.h>
#include <dlfcn.h>
#include "darwin/clock_gettime.cc"

#elif SPRT_WINDOWS
#include "windows/clock_gettime.cc"
#include "windows/sched.cc"
#include "windows/libc.h"
#else
#error Not implemented
#endif

extern "C" {
__SPRT_NORETURN void abort();
}

namespace sprt {

const nothrow_t nothrow;

__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(stdin_impl)() {
	return stdin; //
}
__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(stdout_impl)() {
	return stdout; //
}
__SPRT_C_FUNC __SPRT_ID(FILE) * __SPRT_ID(stderr_impl)() {
	return stderr; //
}

__SPRT_C_FUNC size_t __SPRT_ID(fread_impl)(void *__SPRT_RESTRICT buf, size_t n, size_t count,
		__SPRT_ID(FILE) * __SPRT_RESTRICT file) {
	return ::fread(buf, n, count, file);
}

__SPRT_C_FUNC size_t __SPRT_ID(fwrite_impl)(const void *__SPRT_RESTRICT buf, size_t n, size_t count,
		__SPRT_ID(FILE) * __SPRT_RESTRICT file) {
	return ::fwrite(buf, n, count, file);
}

__SPRT_C_FUNC void __SPRT_ID(perror_impl)(const char *err) { return ::perror(err); }

__SPRT_C_FUNC int __SPRT_ID(fflush_impl)(__SPRT_ID(FILE) * file) { return ::fflush(file); }

__SPRT_C_FUNC int __SPRT_ID(fcntl)(int __fd, int __cmd, ...) {
	unsigned long arg;
	__builtin_va_list ap;
	__builtin_va_start(ap, __cmd);
	arg = __builtin_va_arg(ap, unsigned long);
	__builtin_va_end(ap);

	return fcntl(__fd, __cmd, arg);
}


__SPRT_C_FUNC int __SPRT_ID(usleep)(__SPRT_ID(time_t) __useconds) { return usleep(__useconds); }

__SPRT_C_FUNC void __sprt_assert_fail(const char *cond, const char *file, unsigned int line,
		const char *fn, const char *text) __SPRT_NOEXCEPT {
	auto features = oslog::LogFeatures::acquire();
	__malloc_string prefix;
#if !SPRT_ANDROID
	prefix = StreamTraits<char>::toString<__malloc_string>(features.reverse, features.bold,
			features.fred, "[F]", features.fdef, features.drop);
#endif

	StringView sCond = cond ? StringView(cond) : StringView("<undefined>");
	StringView sFile = file ? StringView(file) : StringView("<file>");
	StringView sFn = fn ? StringView(fn) : StringView("<function>");

	if (text && text[0] != 0) {
		oslog::vprint(oslog::LogType::Fatal, source_location_ext{file, fn, line, 0}, "Assert", sFn,
				": (", sCond, ") failed: ", text, " ", features.underline, features.dim, sFile, ":",
				line, features.drop);
	} else {
		oslog::vprint(oslog::LogType::Fatal, source_location_ext{file, fn, line, 0}, "Assert", sFn,
				": (", sCond, ") failed: ", features.underline, features.dim, sFile, ":", line,
				features.drop);
	}
	::abort();
}

__SPRT_C_FUNC int __SPRT_ID(strverscmp)(const char *l0, const char *r0) {
	// Based on MUSL implementation
	const unsigned char *l = (const unsigned char *)l0;
	const unsigned char *r = (const unsigned char *)r0;
	size_t i, dp, j;
	int z = 1;

	/* Find maximal matching prefix and track its maximal digit
	 * suffix and whether those digits are all zeros. */
	for (dp = i = 0; l[i] == r[i]; i++) {
		int c = l[i];
		if (!c) {
			return 0;
		}
		if (!__constexpr_isdigit_c(c)) {
			dp = i + 1, z = 1;
		} else if (c != '0') {
			z = 0;
		}
	}

	if (l[dp] - '1' < 9U && r[dp] - '1' < 9U) {
		/* If we're looking at non-degenerate digit sequences starting
		 * with nonzero digits, longest digit string is greater. */
		for (j = i; __constexpr_isdigit_c(l[j]); j++) {
			if (!__constexpr_isdigit_c(r[j])) {
				return 1;
			}
		}
		if (__constexpr_isdigit_c(r[j])) {
			return -1;
		}
	} else if (z && dp < i && (__constexpr_isdigit_c(l[i]) || __constexpr_isdigit_c(r[i]))) {
		/* Otherwise, if common prefix of digit sequence is
		 * all zeros, digits order less than non-digits. */
		return (unsigned char)(l[i] - '0') - (unsigned char)(r[i] - '0');
	}

	return l[i] - r[i];
}

__SPRT_C_FUNC int __SPRT_ID(clock_getres)(__SPRT_ID(clockid_t) clock, __SPRT_TIMESPEC_NAME *out) {
	return _clock_getres(clock, out);
}

__SPRT_C_FUNC int __SPRT_ID(clock_gettime)(__SPRT_ID(clockid_t) clock, __SPRT_TIMESPEC_NAME *out) {
	return _clock_gettime(clock, out);
}

__SPRT_C_FUNC int __SPRT_ID(
		clock_settime)(__SPRT_ID(clockid_t) clock, const __SPRT_TIMESPEC_NAME *ts) {
	return _clock_settime(clock, ts);
}

__SPRT_C_FUNC __SPRT_ID(uint64_t) __SPRT_ID(clock_gettime_nsec_np)(__SPRT_ID(clockid_t) __clock) {
	struct __SPRT_TIMESPEC_NAME __tp;
	_clock_gettime(__clock, &__tp);
	return static_cast<uint64_t>(__tp.tv_sec) * static_cast<uint64_t>(1'000'000'000LLU)
			+ static_cast<uint64_t>(__tp.tv_nsec);
}

__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(gettid)(void) {
	auto t = __sprt_pthread_self_noattach_np();
	if (t) {
		__SPRT_ID(pid_t) tid = 0;
		__sprt_pthread_getid_np(t, &tid);
		return tid;
	}
#if SPRT_MACOS
	return pthread_mach_thread_np(pthread_self());
#elif SPRT_WINDOWS
	return GetCurrentThreadId();
#else
	return ::gettid();
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		__sched_cpucount)(__SPRT_ID(size_t) setsize, const __SPRT_ID(cpu_set_t) * set) {
	// Based on bionic: https://android.googlesource.com/platform/bionic/+/234dc36ac/libc/bionic/sched_cpucount.c
	int nn = 0;
	int nn_max = setsize / sizeof(set->__bits[0]);
	int count = 0;
	for (; nn < nn_max; nn++) {
		count += __builtin_popcountg(set->__bits[nn]); //
	}
	return count;
}

__SPRT_C_FUNC int __SPRT_ID(sched_get_priority_max)(int t) { return sched_get_priority_max(t); }

__SPRT_C_FUNC int __SPRT_ID(sched_get_priority_min)(int t) { return sched_get_priority_min(t); }

__SPRT_C_FUNC int __SPRT_ID(sched_yield)(void) { return sched_yield(); }

__SPRT_C_FUNC int __SPRT_ID(dlclose)(void *ptr) { return dlclose(ptr); }

__SPRT_C_FUNC char *__SPRT_ID(dlerror)(void) { return dlerror(); }

__SPRT_C_FUNC void *__SPRT_ID(dlopen)(const char *path, int __flags) {
	return dlopen(path, __flags);
}

__SPRT_C_FUNC void *__SPRT_ID(
		dlsym)(void *__SPRT_RESTRICT __handle, const char *__SPRT_RESTRICT __name) {
	return dlsym(__handle, __name);
}

__SPRT_C_FUNC int __SPRT_ID(dladdr)(const void *__handle, __SPRT_ID(Dl_info) * __info) {
	return dladdr(__handle, (Dl_info *)__info);
}

__SPRT_C_FUNC double __SPRT_ID(log2_impl)(double value) { return ::log2(value); }

__SPRT_C_FUNC float __SPRT_ID(log2f_impl)(float value) { return ::log2f(value); }

__SPRT_C_FUNC long double __SPRT_ID(log2l_impl)(long double value) { return ::log2l(value); }

__SPRT_C_FUNC char *__SPRT_ID(strerror_impl)(int err) { return ::strerror(err); }

__SPRT_C_FUNC __SPRT_ID(errno_t)
		__SPRT_ID(strerror_s)(char *buf, __SPRT_ID(rsize_t) bufsz, __SPRT_ID(errno_t) errnum) {
#if __STDC_LIB_EXT1__
	return ::strerror_s(buf, bufsz, errnum);
#else
	return ::strerror_r(errnum, buf, bufsz);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(tolower_impl)(int c) {
	if (c > 0 && c <= 0x7F) {
		return int(sprt::__constexpr_tolower_c(char(c)));
	}
	return c;
}

__SPRT_C_FUNC int __SPRT_ID(toupper_impl)(int c) {
	if (c > 0 && c <= 0x7F) {
		return int(sprt::__constexpr_toupper_c(char(c)));
	}
	return c;
}

__SPRT_C_FUNC void __SPRT_ID(qsort_impl)(void *array, size_t n, size_t size,
		int (*comparator)(const void *lhs, const void *rhs)) {
	qsort(array, n, size, comparator);
}

__SPRT_C_FUNC void *__SPRT_ID(malloc_impl)(size_t size) __SPRT_NOEXCEPT { return ::malloc(size); }

__SPRT_C_FUNC void *__SPRT_ID(calloc_impl)(size_t n, size_t size) __SPRT_NOEXCEPT {
	return ::calloc(n, size);
}

__SPRT_C_FUNC void *__SPRT_ID(realloc_impl)(void *ptr, size_t size) __SPRT_NOEXCEPT {
	return ::realloc(ptr, size);
}

__SPRT_C_FUNC void __SPRT_ID(free_impl)(void *ptr) __SPRT_NOEXCEPT { return ::free(ptr); }

__SPRT_C_FUNC void __SPRT_ID(free_sized)(void *ptr, size_t size) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return ::free_sized(ptr, size);
#else
	return ::free(ptr);
#endif
}

__SPRT_C_FUNC void __SPRT_ID(
		free_aligned_sized)(void *ptr, size_t alignment, size_t size) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return ::free_aligned_sized(ptr, alignment, size);
#else
	return ::free(ptr);
#endif
}

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(abort_impl)(void) { ::abort(); }

} // namespace sprt


#ifndef SPRT_WINDOWS
#include <sys/utsname.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(uname)(struct __SPRT_UTSNAME_NAME *buf) {
	return ::uname((struct utsname *)buf);
}

} // namespace sprt

#else

#include "windows/uname.cc"

#endif
