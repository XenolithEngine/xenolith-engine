//===----------------------------------------------------------------------===//
//
// sprt overlay for libc++'s mbstate_t acquisition.
//
// The vendored <__mbstate_t.h> probes the platform for the header that defines
// ::mbstate_t (<bits/types/mbstate_t.h> on glibc, <sys/_types/_mbstate_t.h> on
// Darwin, ...). Under this port the libc surface is sprt's: probing the target
// sysroot behind its back drags foreign type definitions into TUs whose libc is
// sprt (on the Apple "+open" sysroot that redefines __mbstate_t already provided
// by sprt's <wchar.h>). libc++ must take mbstate_t from the sprt libc only.
//
// - hosted __SPRT_BUILD TUs (the runtime itself over the platform libc): go
//   through <wchar.h>, whose include_libc dispatch forwards to the platform
//   header - mbstate_t stays the platform's, consistent with the TU's libc.
// - everything else (apps, the runtime_libcxx module): the sprt definition,
//   with no sysroot probing at all.
//
// This file shadows the vendored one (include_libc/cxx precedes libcxx/include
// on every libc++ include chain) and keeps its include guard.
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___MBSTATE_T_H
#define _LIBCPP___MBSTATE_T_H

#if defined(__cplusplus)
#  define __CORRECT_ISO_CPP_WCHAR_H_PROTO
#endif

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include <wchar.h>

#else

#include <sprt/c/cross/__sprt_mbstate.h>

#if !__SPRT_MBSTATE_DIRECT
// see include_libc/wchar.h: _MBSTATE_T coordinates the typedef with Darwin headers
#ifndef _MBSTATE_T
#define _MBSTATE_T
typedef __SPRT_MBSTATE_NAME mbstate_t;
#endif
#endif

#endif // __SPRT_BUILD hosted

#endif // _LIBCPP___MBSTATE_T_H
