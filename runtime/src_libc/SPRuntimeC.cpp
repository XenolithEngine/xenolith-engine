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

#include <sprt/c/__sprt_assert.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_fenv.h>
#include <sprt/c/__sprt_signal.h>
#include <sprt/c/__sprt_setjmp.h>
#include <sprt/c/__sprt_utime.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_locale.h>
#include <sprt/c/__sprt_nl_types.h>
#include <sprt/c/sys/__sprt_utsname.h>

#include <sprt/runtime/math.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/source_location.h>
#include "private/SPRTFilename.h"

#include <stddef.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <float.h>
#include <fenv.h>
#include <signal.h>
#include <setjmp.h>

#ifndef SPRT_WINDOWS

#include <utime.h>
#include <nl_types.h>
#include <sys/utsname.h>
#else
#include <sys/utime.h>
#endif

#include "private/SPRTSpecific.h"

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
	auto features = log::LogFeatures::acquire();
	memory::dynstring prefix;
#if !SPRT_ANDROID
	prefix = StreamTraits<char>::toString(features.reverse, features.bold, features.fred, "[F]",
			features.fdef, features.drop);
#endif

	StringView sCond = cond ? StringView(cond) : StringView("<undefined>");
	StringView sFile = file ? StringView(file) : StringView("<file>");
	StringView sFn = fn ? StringView(fn) : StringView("<function>");

	if (text && text[0] != 0) {
		log::vprint(sprt::log::LogType::Fatal, source_location_ext{file, fn, line, 0}, "Assert",
				sFn, ": (", sCond, ") failed: ", text, " ", features.underline, features.dim, sFile,
				":", line, features.drop);
	} else {
		log::vprint(sprt::log::LogType::Fatal, source_location_ext{file, fn, line, 0}, "Assert",
				sFn, ": (", sCond, ") failed: ", features.underline, features.dim, sFile, ":", line,
				features.drop);
	}
	::abort();
}

__SPRT_C_FUNC __SPRT_FALLBACK_ATTR(const) int *__SPRT_ID(__errno_location)(void) {
#if SPRT_LINUX
	return ::__errno_location();
#elif SPRT_ANDROID
	return ::__errno();
#elif SPRT_WINDOWS
	return ::_errno();
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

__SPRT_C_FUNC int __SPRT_ID(sigemptyset)(__SPRT_ID(sigset_t) * set) {
	for (auto &it : set->__bits) { it = 0; }
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(sigfillset)(__SPRT_ID(sigset_t) * set) {
#if SPRT_WINDOWS
	set->__bits[0] = 0xffff'ffff;
	return 0;
#else
	return ::sigfillset((sigset_t *)set);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(sigaddset)(__SPRT_ID(sigset_t) * set, int s) {
#if SPRT_WINDOWS
	if (s < __SPRT__NSIG) {
		set->__bits[0] |= (1 << s);
	}
	*__sprt___errno_location() = EINVAL;
	return -1;
#else
	return ::sigaddset((sigset_t *)set, s);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(sigdelset)(__SPRT_ID(sigset_t) * set, int s) {
#if SPRT_WINDOWS
	if (s < __SPRT__NSIG) {
		set->__bits[0] &= ~(1 << s);
		return 0;
	}
	*__sprt___errno_location() = EINVAL;
	return -1;
#else
	return ::sigdelset((sigset_t *)set, s);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(sigismember)(const __SPRT_ID(sigset_t) * set, int s) {
#if SPRT_WINDOWS
	if (s < __SPRT__NSIG) {
		return !!(set->__bits[0] & (1 << s));
	}
	return 0;
#else
	return ::sigismember((const sigset_t *)set, s);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(sigprocmask)(int m, const __SPRT_ID(sigset_t) * __SPRT_RESTRICT a,
		__SPRT_ID(sigset_t) * __SPRT_RESTRICT b) {
#if __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK
	return ::sigprocmask(m, (const sigset_t *)a, (sigset_t *)b);
#else
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(sigsuspend)(const __SPRT_ID(sigset_t) * set) {
#if __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK
	return ::sigsuspend((const sigset_t *)set);
#else
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(sigpending)(__SPRT_ID(sigset_t) * set) {
#if __SPRT_CONFIG_HAVE_SIGNAL_SIGPROCMASK
	return ::sigpending((sigset_t *)set);
#else
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(sigisemptyset)(const __SPRT_ID(sigset_t) * set) {
	for (auto &it : set->__bits) {
		if (it != 0) {
			return 0;
		}
	}
	return 1;
}

__SPRT_C_FUNC int __SPRT_ID(sigorset)(__SPRT_ID(sigset_t) * set, const __SPRT_ID(sigset_t) * a,
		const __SPRT_ID(sigset_t) * b) {
	auto s = sizeof(set->__bits) / sizeof(set->__bits[0]);
	for (size_t i = 0; i < s; ++i) { set->__bits[i] = a->__bits[i] | b->__bits[i]; }
	return 0;
}
__SPRT_C_FUNC int __SPRT_ID(sigandset)(__SPRT_ID(sigset_t) * set, const __SPRT_ID(sigset_t) * a,
		const __SPRT_ID(sigset_t) * b) {
	auto s = sizeof(set->__bits) / sizeof(set->__bits[0]);
	for (size_t i = 0; i < s; ++i) { set->__bits[i] = a->__bits[i] & b->__bits[i]; }
	return 0;
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
#elif SPRT_WINDOWS
__SPRT_C_FUNC int __SPRT_ID(setjmp)(__SPRT_ID(jmp_buf) buf) { return ::setjmp((_JBTYPE *)buf); }

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(longjmp)(__SPRT_ID(jmp_buf) buf, int ret) {
	::longjmp((_JBTYPE *)buf, ret);
}
#else
__SPRT_C_FUNC int __SPRT_ID(setjmp)(__SPRT_ID(jmp_buf) buf) { return ::setjmp(buf); }

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(longjmp)(__SPRT_ID(jmp_buf) buf, int ret) {
	::longjmp(buf, ret);
}
#endif


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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#else
	return ::catopen(path, v);
#endif
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#else
	return ::catgets(cat, a, b, str);
#endif
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::catclose(cat);
#endif
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_NLTYPES_CAT)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

#ifndef SPRT_WINDOWS
__SPRT_C_FUNC int __SPRT_ID(uname)(struct __SPRT_UTSNAME_NAME *buf) {
	return ::uname((struct utsname *)buf);
}
#endif

} // namespace sprt


#if SPRT_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(uname)(struct __SPRT_UTSNAME_NAME *buf) {
	OSVERSIONINFOEXA ver;
	ZeroMemory(&ver, sizeof(OSVERSIONINFOEXA));
	ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);

	ZeroMemory(buf, sizeof(struct __SPRT_UTSNAME_NAME));

	memcpy(buf->sysname, "Windows", 8);

	DWORD bufSize = 64;
	GetComputerNameA(buf->domainname, &bufSize);

	SYSTEM_INFO sysinfo;
	GetNativeSystemInfo(&sysinfo);

	switch (sysinfo.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL: memcpy(buf->machine, "i386", 4); break;
	case PROCESSOR_ARCHITECTURE_MIPS: memcpy(buf->machine, "mips", 4); break;
	case PROCESSOR_ARCHITECTURE_ALPHA: memcpy(buf->machine, "alpha", 5); break;
	case PROCESSOR_ARCHITECTURE_PPC: memcpy(buf->machine, "ppc", 3); break;
	case PROCESSOR_ARCHITECTURE_SHX: memcpy(buf->machine, "shx", 3); break;
	case PROCESSOR_ARCHITECTURE_ARM: memcpy(buf->machine, "arm", 3); break;
	case PROCESSOR_ARCHITECTURE_IA64: memcpy(buf->machine, "ia64", 4); break;
	case PROCESSOR_ARCHITECTURE_ALPHA64: memcpy(buf->machine, "alpha64", 7); break;
	case PROCESSOR_ARCHITECTURE_MSIL: memcpy(buf->machine, "msil", 4); break;
	case PROCESSOR_ARCHITECTURE_AMD64: memcpy(buf->machine, "x86_64", 5); break;
	case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64: memcpy(buf->machine, "i386", 4); break;
	case PROCESSOR_ARCHITECTURE_NEUTRAL: memcpy(buf->machine, "neutral", 7); break;
	case PROCESSOR_ARCHITECTURE_ARM64: memcpy(buf->machine, "aarch64", 7); break;
	case PROCESSOR_ARCHITECTURE_ARM32_ON_WIN64: memcpy(buf->machine, "arm", 3); break;
	case PROCESSOR_ARCHITECTURE_IA32_ON_ARM64: memcpy(buf->machine, "i386", 4); break;
	}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	if (GetVersionExA((LPOSVERSIONINFOA)&ver)) {
		StreamTraits<char>::toStringBuf(buf->release, 64, int64_t(ver.dwMajorVersion), ".",
				int64_t(ver.dwMinorVersion), ".", int64_t(ver.dwBuildNumber));

		StreamTraits<char>::toStringBuf(buf->version, 64, ver.wServicePackMajor, ".",
				ver.wServicePackMinor, " (p:", ver.wSuiteMask, ")");
		return 0;
	}
#pragma clang diagnostic pop
	return -1;
}

} // namespace sprt

#endif
