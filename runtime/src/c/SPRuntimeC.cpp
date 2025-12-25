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

#include <c/__sprt_assert.h>
#include <c/__sprt_errno.h>
#include <c/__sprt_fenv.h>
#include <c/__sprt_signal.h>
#include <c/__sprt_setjmp.h>
#include <c/__sprt_utime.h>
#include <c/__sprt_stdio.h>
#include <c/__sprt_locale.h>
#include <c/__sprt_nl_types.h>

#include "SPRuntimeLog.h"
#include "SPRuntimeStringBuffer.h"
#include "private/SPRTFilename.h"

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <float.h>
#include <fenv.h>
#include <signal.h>
#include <setjmp.h>
#include <utime.h>
#include <nl_types.h>

namespace sprt {

__SPRT_C_FUNC void __sprt_assert_fail(const char *cond, const char *file, unsigned int line,
		const char *fn, const char *text) __SPRT_NOEXCEPT {
	auto features = log::LogFeatures::acquire();
	StringBuffer<char> prefix;
#if !SPRT_ANDROID
	prefix = StringBuffer<char>::create(features.reverse, features.bold, features.fred, "[F]",
			features.fdef, features.drop);
#endif

	StringView sCond = cond ? StringView(cond) : StringView("<undefined>");
	StringView sFile = file ? StringView(file) : StringView("<file>");
	StringView sFn = fn ? StringView(fn) : StringView("<function>");

	if (text && text[0] != 0) {
		log::vprint(sprt::log::LogType::Fatal, log::SourceLocation{file, fn, line}, "Assert", sFn,
				": (", sCond, ") failed: ", text, " ", features.underline, features.dim, sFile, ":",
				line, features.drop);
	} else {
		log::vprint(sprt::log::LogType::Fatal, log::SourceLocation{file, fn, line}, "Assert", sFn,
				": (", sCond, ") failed: ", features.underline, features.dim, sFile, ":", line,
				features.drop);
	}
	::abort();
}

__SPRT_C_FUNC __SPRT_FALLBACK_ATTR(const) int *__SPRT_ID(__errno_location)(void) {
#if SPRT_LINUX
	return ::__errno_location();
#elif SPRT_ANDROID
	return ::__errno();
#else
	return ::__errno_location();
#endif
}

__SPRT_C_FUNC int __SPRT_ID(__flt_rounds)(void) { return FLT_ROUNDS; }


__SPRT_C_FUNC char *__SPRT_ID(setlocale)(int cat, const char *locale) {
	return ::setlocale(cat, locale);
}

__SPRT_C_FUNC struct __SPRT_ID(lconv) * __SPRT_ID(localeconv)(void) {
	return (struct __SPRT_ID(lconv) *)::localeconv();
}

__SPRT_C_FUNC __SPRT_ID(locale_t) __SPRT_ID(duplocale)(__SPRT_ID(locale_t) loc) {
	return ::duplocale(loc);
}
__SPRT_C_FUNC void __SPRT_ID(freelocale)(__SPRT_ID(locale_t) loc) { ::freelocale(loc); }
__SPRT_C_FUNC __SPRT_ID(locale_t)
		__SPRT_ID(newlocale)(int v, const char *name, __SPRT_ID(locale_t) loc) {
	return ::newlocale(v, name, loc);
}
__SPRT_C_FUNC __SPRT_ID(locale_t) __SPRT_ID(uselocale)(__SPRT_ID(locale_t) loc) {
	return ::uselocale(loc);
}


__SPRT_C_FUNC __sprt_fenv_t *__sprt_arch_FE_DFL_ENV_fn() { return (__sprt_fenv_t *)FE_DFL_ENV; }

__SPRT_C_FUNC int __SPRT_ID(feclearexcept)(int v) { return ::feclearexcept(v); }
__SPRT_C_FUNC int __SPRT_ID(fegetexceptflag)(__SPRT_ID(fexcept_t) * ex, int v) {
	return ::fegetexceptflag(ex, v);
}
__SPRT_C_FUNC int __SPRT_ID(feraiseexcept)(int v) { return ::feraiseexcept(v); }
__SPRT_C_FUNC int __SPRT_ID(fesetexceptflag)(const __SPRT_ID(fexcept_t) * ex, int v) {
	return ::fesetexceptflag(ex, v);
}
__SPRT_C_FUNC int __SPRT_ID(fetestexcept)(int v) { return ::fetestexcept(v); }

__SPRT_C_FUNC int __SPRT_ID(fegetround)(void) { return ::fegetround(); }
__SPRT_C_FUNC int __SPRT_ID(fesetround)(int v) { return ::fesetround(v); }

__SPRT_C_FUNC int __SPRT_ID(fegetenv)(__SPRT_ID(fenv_t) * ex) { return ::fegetenv((fenv_t *)ex); }
__SPRT_C_FUNC int __SPRT_ID(feholdexcept)(__SPRT_ID(fenv_t) * ex) {
	return ::feholdexcept((fenv_t *)ex);
}
__SPRT_C_FUNC int __SPRT_ID(fesetenv)(const __SPRT_ID(fenv_t) * ex) {
	return ::fesetenv((const fenv_t *)ex);
}
__SPRT_C_FUNC int __SPRT_ID(feupdateenv)(const __SPRT_ID(fenv_t) * ex) {
	return ::feupdateenv((const fenv_t *)ex);
}

__SPRT_C_FUNC void (*__SPRT_ID(signal)(int sig, void (*cb)(int)))(int) { return ::signal(sig, cb); }

__SPRT_C_FUNC int __SPRT_ID(raise)(int sig) { return ::raise(sig); }

#if SPRT_LINUX
__SPRT_C_FUNC int __SPRT_ID(setjmp)(__SPRT_ID(jmp_buf) buf) {
	return ::setjmp((struct __jmp_buf_tag *)buf);
}

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(longjmp)(__SPRT_ID(jmp_buf) buf, int ret) {
	::longjmp((struct __jmp_buf_tag *)buf, ret);
}
#else
__SPRT_C_FUNC int __SPRT_ID(setjmp)(__SPRT_ID(jmp_buf) buf) { return ::setjmp(buf); }

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(longjmp)(__SPRT_ID(jmp_buf) buf, int ret) {
	::longjmp(buf, ret);
}
#endif


__SPRT_C_FUNC int __SPRT_ID(utime)(const char *path, const struct __SPRT_UTIMBUF_NAME *buf) {
	struct utimbuf nativeBuf;
	if (buf) {
		nativeBuf.actime = buf->actime;
		nativeBuf.modtime = buf->modtime;
	}

	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::utime(target, buf ? &nativeBuf : nullptr);
	}, -1);
}

__SPRT_C_FUNC __SPRT_ID(nl_catd) __SPRT_ID(catopen)(const char *path, int v) {
	return ::catopen(path, v);
}

__SPRT_C_FUNC char *__SPRT_ID(catgets)(__SPRT_ID(nl_catd) cat, int a, int b, const char *str) {
	return ::catgets(cat, a, b, str);
}

__SPRT_C_FUNC int __SPRT_ID(catclose)(__SPRT_ID(nl_catd) cat) { return ::catclose(cat); }

} // namespace sprt
