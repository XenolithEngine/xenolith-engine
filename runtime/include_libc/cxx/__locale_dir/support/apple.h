//===----------------------------------------------------------------------===//
//
// sprt overlay for libc++'s locale backend selection (Apple targets).
//
// libc++ picks its locale support backend in <__locale_dir/locale_base_api.h> by
// platform macro: on the *-apple-* triples __APPLE__ is defined, so it selects
// <__locale_dir/support/apple.h>, which in turn pulls <__locale_dir/support/bsd_like.h>
// -- a backend written against the Darwin/BSD xlocale surface (the ::mbtowc_l /
// ::mbrlen_l / ::mbsrtowcs_l "_l" family, ::__darwin_mbstate_t, MB_CUR_MAX_L, ...).
//
// sprt is NOT the Darwin libc. It is a POSIX-flavored libc that exposes the standard
// POSIX locale surface on EVERY target: locale_t + newlocale / freelocale / uselocale /
// duplocale, and the POSIX (un-prefixed) *_l functions strtod_l / isupper_l / toupper_l /
// strcoll_l / strxfrm_l / iswctype_l / ... -- which is exactly the contract of libc++'s
// POSIX backend <__locale_dir/support/linux.h>. The multibyte entry points that
// bsd_like.h expects in their locale-parameterized "_l" spelling (mbtowc_l, mbrlen_l,
// mbsrtowcs_l, wcrtomb_l, ...) are NOT part of POSIX and sprt does not provide them; the
// POSIX backend instead runs the plain mbtowc / mbrlen / ... under a uselocale guard,
// which sprt does provide. That backend is what the sprt Linux target already uses and
// validates against the conformance suite, and it pulls only standard headers (no
// Linux-kernel dependency and no __linux__ guard).
//
// This overlay file shadows the vendored apple.h (the overlay dir precedes
// libcxx/include on the search path) purely to redirect the Darwin/BSD backend selection
// to the POSIX one, so the Apple targets reuse the same, working sprt locale wiring
// instead of the unavailable Darwin xlocale API. Same rationale and same shape as the
// windows.h overlay next to it. It is inert on every other target: the vendored dispatch
// only reaches apple.h under __APPLE__, so on non-Apple triples this file is never
// included.
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___LOCALE_DIR_SUPPORT_APPLE_H
#define _LIBCPP___LOCALE_DIR_SUPPORT_APPLE_H

#include <__locale_dir/support/linux.h>

#endif // _LIBCPP___LOCALE_DIR_SUPPORT_APPLE_H
