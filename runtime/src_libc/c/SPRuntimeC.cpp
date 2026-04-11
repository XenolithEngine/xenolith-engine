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
#define _CRT_STDIO_ISO_WIDE_SPECIFIERS 1 // for libc++ compatibility

#include <sprt/c/__sprt_assert.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_utime.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_locale.h>
#include <sprt/c/__sprt_nl_types.h>

#include <sprt/runtime/math.h>
#include <sprt/runtime/log.h>
#include "private/SPRTFilename.h"

#include <stddef.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>

#ifndef SPRT_WINDOWS
#include <utime.h>
#include <nl_types.h>
#else
#include <sys/utime.h>
#endif

#include "private/SPRTSpecific.h"

#include "common/errno.cc"
#include "common/fenv.cc"
#include "common/signal.cc"
#include "common/uname.cc"

#if SPRT_ANDROID
namespace sprt::platform {

extern nl_catd (*_catopen)(const char *__name, int __flag);
extern char *(*_catgets)(nl_catd __catalog, int __set_number, int __msg_number, const char *__msg);
extern int (*_catclose)(nl_catd __catalog);

} // namespace sprt::platform
#endif

namespace sprt {

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

__SPRT_C_FUNC int __SPRT_ID(__flt_rounds)(void) { return FLT_ROUNDS; }

__SPRT_C_FUNC char *__SPRT_ID(setlocale)(int cat, const char *locale) {
	return ::setlocale(cat, locale);
}

__SPRT_C_FUNC struct __SPRT_ID(lconv) * __SPRT_ID(localeconv)(void) {
	return (struct __SPRT_ID(lconv) *)::localeconv();
}

__SPRT_C_FUNC __SPRT_ID(locale_t) __SPRT_ID(duplocale)(__SPRT_ID(locale_t) loc) {
#if !__SPRT_CONFIG_HAVE_LOCALE_EXT
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#else
	return ::duplocale(loc);
#endif
}
__SPRT_C_FUNC void __SPRT_ID(freelocale)(__SPRT_ID(locale_t) loc) {
#if __SPRT_CONFIG_HAVE_LOCALE_EXT
	::freelocale(loc);
#endif
}
__SPRT_C_FUNC __SPRT_ID(locale_t)
		__SPRT_ID(newlocale)(int v, const char *name, __SPRT_ID(locale_t) loc) {
#if !__SPRT_CONFIG_HAVE_LOCALE_EXT
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#else
	return ::newlocale(v, name, loc);
#endif
}
__SPRT_C_FUNC __SPRT_ID(locale_t) __SPRT_ID(uselocale)(__SPRT_ID(locale_t) loc) {
#if !__SPRT_CONFIG_HAVE_LOCALE_EXT
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#else
	return ::uselocale(loc);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(sigemptyset)(__SPRT_ID(sigset_t) * set) {
	for (auto &it : set->__bits) { it = 0; }
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(utime)(const char *path, const struct __SPRT_UTIMBUF_NAME *buf) {
	return internal::performWithNativePath(path, [&](const char *target) {
#if SPRT_WINDOWS
		struct __utimbuf64 nativeBuf;
		if (buf) {
			nativeBuf.actime = buf->actime;
			nativeBuf.modtime = buf->modtime;
		}
		return ::_utime64(target, buf ? &nativeBuf : nullptr);
#else
		struct utimbuf nativeBuf;
		if (buf) {
			nativeBuf.actime = buf->actime;
			nativeBuf.modtime = buf->modtime;
		}
		// call with native path
		return ::utime(target, buf ? &nativeBuf : nullptr);
#endif
	}, -1);
}

__SPRT_C_FUNC __SPRT_ID(nl_catd) __SPRT_ID(catopen)(const char *path, int v) {
#if __SPRT_CONFIG_HAVE_NLTYPES_CAT
#if SPRT_ANDROID
	if (platform::_catopen) {
		return platform::_catopen(path, v);
	}
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#else
	return ::catopen(path, v);
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_NLTYPES_CAT)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#endif
}

__SPRT_C_FUNC char *__SPRT_ID(catgets)(__SPRT_ID(nl_catd) cat, int a, int b, const char *str) {
#if __SPRT_CONFIG_HAVE_NLTYPES_CAT
#if SPRT_ANDROID
	if (platform::_catgets) {
		return platform::_catgets(cat, a, b, str);
	}
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#elif SPRT_MACOS
	return ::catgets(nl_catd(cat), a, b, str);
#else
	return ::catgets(cat, a, b, str);
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_NLTYPES_CAT)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(catclose)(__SPRT_ID(nl_catd) cat) {
#if __SPRT_CONFIG_HAVE_NLTYPES_CAT
#if SPRT_ANDROID
	if (platform::_catclose) {
		return platform::_catclose(cat);
	}
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_MACOS
	return ::catclose(nl_catd(cat));
#else
	return ::catclose(cat);
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_NLTYPES_CAT)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

} // namespace sprt
