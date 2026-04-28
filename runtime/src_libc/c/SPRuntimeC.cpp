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
#include <sprt/c/__sprt_nl_types.h>

#include <sprt/runtime/math.h>
#include <sprt/runtime/log.h>
#include "private/SPRTFilename.h"

#if __STDC_HOSTED__ == 0

#else

#include <stddef.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <utime.h>
#include <nl_types.h>

#endif

#include "private/SPRTSpecific.h"

#include "common/errno.cc"
#include "common/fenv.cc"
#include "common/signal.cc"
#include "common/uname.cc"
#include "common/abort.cc"
#include "common/locale.cc"

#if SPRT_ANDROID
namespace sprt::platform {

extern nl_catd (*_catopen)(const char *__name, int __flag);
extern char *(*_catgets)(nl_catd __catalog, int __set_number, int __msg_number, const char *__msg);
extern int (*_catclose)(nl_catd __catalog);

} // namespace sprt::platform
#endif

namespace sprt {

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
