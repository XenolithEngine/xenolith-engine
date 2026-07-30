//===----------------------------------------------------------------------===//
//
// sprt overlay for libc++'s locale backend selection (see windows.h next to this
// file for the full rationale).
//
// libc++ picks its locale support backend in <__locale_dir/locale_base_api.h> by
// platform macro: under __APPLE__ it selects <__locale_dir/support/apple.h> -- a
// backend written against Darwin's <xlocale.h> surface (the BSD "_l" family:
// ::localeconv_l / ::btowc_l / ::wcrtomb_l / MB_CUR_MAX_L / ...).
//
// sprt is not Darwin's libc. It exposes the standard POSIX locale surface on EVERY
// target -- exactly the contract of libc++'s POSIX backend
// <__locale_dir/support/linux.h> (which, despite the name, pulls only standard
// headers and has no __linux__ guard). Redirect the Apple selection to it, so the
// macOS/iOS targets reuse the same sprt locale wiring as Linux and Windows.
//
// This overlay shadows the vendored apple.h (the overlay dir precedes
// libcxx/include on the search path). It is inert on every other target: the
// vendored dispatch only reaches apple.h under __APPLE__.
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___LOCALE_DIR_SUPPORT_APPLE_H
#define _LIBCPP___LOCALE_DIR_SUPPORT_APPLE_H

#include <__locale_dir/support/linux.h>

#endif // _LIBCPP___LOCALE_DIR_SUPPORT_APPLE_H
