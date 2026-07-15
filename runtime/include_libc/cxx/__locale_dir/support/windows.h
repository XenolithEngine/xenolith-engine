//===----------------------------------------------------------------------===//
//
// sprt overlay for libc++'s locale backend selection.
//
// libc++ picks its locale support backend in <__locale_dir/locale_base_api.h> by
// platform macro: on the *-windows-msvc triple _LIBCPP_MSVCRT_LIKE is defined, so it
// selects <__locale_dir/support/windows.h> -- a backend written against the MSVC C
// runtime's locale surface (::_locale_t, ::_create_locale / ::_free_locale, the
// ::_isupper_l / ::_strtod_l / ::_strcoll_l "_l" family, ::___lc_codepage_func, ...).
//
// sprt is NOT the MSVC runtime. It is a POSIX-flavored libc that exposes the standard
// POSIX locale surface on EVERY target: locale_t + newlocale / freelocale / uselocale /
// duplocale, and the POSIX (un-prefixed) *_l functions strtod_l / isupper_l / toupper_l /
// strcoll_l / strxfrm_l / iswctype_l / ... -- which is exactly the contract of libc++'s
// POSIX backend <__locale_dir/support/linux.h>. That backend is what the sprt Linux
// target already uses and validates against the conformance suite, and it pulls only
// standard headers (no Linux-kernel dependency and no __linux__ guard).
//
// This overlay file shadows the vendored windows.h (the overlay dir precedes
// libcxx/include on the search path) purely to redirect the MSVCRT backend selection to
// the POSIX one, so the Windows target reuses the same, working sprt locale wiring
// instead of the unavailable MSVCRT _locale_t / _*_l API. It is inert on every other
// target: the vendored dispatch only reaches windows.h under _LIBCPP_MSVCRT_LIKE, so on
// non-Microsoft triples this file is never included.
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___LOCALE_DIR_SUPPORT_WINDOWS_H
#define _LIBCPP___LOCALE_DIR_SUPPORT_WINDOWS_H

#include <__locale_dir/support/linux.h>

#endif // _LIBCPP___LOCALE_DIR_SUPPORT_WINDOWS_H
