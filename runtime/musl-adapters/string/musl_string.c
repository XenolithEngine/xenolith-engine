#define __SPRT_BUILD
// The bundled musl string sources call the BSD/GNU extensions (strlcpy/strlcat,
// wcscasecmp/wcsncasecmp, ...), which musl's <string.h>/<wchar.h> only declare
// under a feature-test macro. _GNU_SOURCE enables them all. (Windows skips these
// sources via the !SPRT_WINDOWS guard below, so this only affects the freestanding
// targets that actually compile them.)
#define _GNU_SOURCE

#include "../include/defs.h"

#if !SPRT_WINDOWS
#include "../../musl-libc/src/string/memcmp.c"
#include "../../musl-libc/src/string/strcpy.c"
#include "../../musl-libc/src/string/strnlen.c"
#include "../../musl-libc/src/string/strncpy.c"
#include "../../musl-libc/src/string/strstr.c"
#include "../../musl-libc/src/string/strchr.c"
#include "../../musl-libc/src/string/strcmp.c"
#include "../../musl-libc/src/string/strncmp.c"
#include "../../musl-libc/src/string/strlen.c"
// The wcs* collation/compare entry points are owned by the freestanding libc's
// own builtin_wchar.cpp / builtin_locale.cpp (they route through the locale
// backend). Windows relies on that; wasm is freestanding the same way, so it
// skips the musl versions here to avoid duplicate symbols. wcsstr is the one
// exception — the builtins do not provide it, so musl supplies it everywhere.
#if !SPRT_WASM
#include "../../musl-libc/src/string/wcscasecmp_l.c"
#include "../../musl-libc/src/string/wcsncasecmp_l.c"
#include "../../musl-libc/src/string/wcscasecmp.c"
#include "../../musl-libc/src/string/wcsncasecmp.c"
#include "../../musl-libc/src/string/wcscmp.c"
#include "../../musl-libc/src/string/wcsncmp.c"
#endif
// wcslen/wcscpy/wcsncpy/wcsnlen are the plain copy/length helpers: builtin_wchar.cpp
// owns only the *cmp/*casecmp collation entry points, and USES these four without
// defining them, so musl must supply them on every target (wasm included).
#include "../../musl-libc/src/string/wcsnlen.c"
#include "../../musl-libc/src/string/wcscpy.c"
#include "../../musl-libc/src/string/wcslen.c"
#include "../../musl-libc/src/string/wcsncpy.c"
#include "../../musl-libc/src/string/wcsstr.c"
#endif

#pragma clang diagnostic ignored "-Wunused-label"
#pragma clang diagnostic ignored "-Wunused-variable"

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64
#elif __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64
#else
#include "../../musl-libc/src/string/memcpy.c"
#include "../../musl-libc/src/string/memmove.c"
#include "../../musl-libc/src/string/memset.c"
#endif

#include "../../musl-libc/src/string/bcmp.c"
#include "../../musl-libc/src/string/bcopy.c"
#include "../../musl-libc/src/string/bzero.c"
#include "../../musl-libc/src/string/explicit_bzero.c"
#include "../../musl-libc/src/string/index.c"
#include "../../musl-libc/src/string/memccpy.c"
#include "../../musl-libc/src/string/memchr.c"
#include "../../musl-libc/src/string/memmem.c"
#include "../../musl-libc/src/string/mempcpy.c"
#include "../../musl-libc/src/string/memrchr.c"
#include "../../musl-libc/src/string/rindex.c"
#include "../../musl-libc/src/string/stpncpy.c"
#include "../../musl-libc/src/string/strcasecmp.c"
#include "../../musl-libc/src/string/strcasestr.c"
#include "../../musl-libc/src/string/strcat.c"
#include "../../musl-libc/src/string/strcspn.c"
#include "../../musl-libc/src/string/strdup.c"
#include "../../musl-libc/src/string/strerror_r.c"
#include "../../musl-libc/src/string/strlcat.c"

#include "../../musl-libc/src/string/strlcpy.c"
#undef ALIGN
#undef ONES
#undef HIGHS
#undef HASZERO

#include "../../musl-libc/src/string/strncasecmp.c"
#include "../../musl-libc/src/string/strncat.c"
#include "../../musl-libc/src/string/strndup.c"
#include "../../musl-libc/src/string/strpbrk.c"
#include "../../musl-libc/src/string/strrchr.c"
#include "../../musl-libc/src/string/strsep.c"
#include "../../musl-libc/src/string/strspn.c"
#include "../../musl-libc/src/string/strtok.c"
#include "../../musl-libc/src/string/strtok_r.c"
#include "../../musl-libc/src/string/strverscmp.c"

#ifndef offsetof
#define offsetof(type, member) ((size_t) & (((type *)NULL)->member))
#endif

static const struct errmsgstr_t {
#define E(n, s) char str##n[sizeof(s)];
#include "../../musl-libc/src/errno/__strerror.h"
#undef E
} errmsgstr = {
#define E(n, s) s,
#include "../../musl-libc/src/errno/__strerror.h"
#undef E
};

static const unsigned short errmsgidx[] = {
#define E(n, s) [n] = offsetof(struct errmsgstr_t, str##n),
#include "../../musl-libc/src/errno/__strerror.h"
#undef E
};

char *strerror(int e) {
	const char *s;
	if (e >= sizeof errmsgidx / sizeof *errmsgidx) {
		e = 0;
	}
	s = (const char *)&errmsgstr + errmsgidx[e];
	return (char *)s;
}

#include "../../musl-libc/src/string/stpcpy.c"
#undef ALIGN
#undef ONES
#undef HIGHS
#undef HASZERO

#include "../../musl-libc/src/string/strchrnul.c"

#include "../../musl-libc/src/string/swab.c"
#include "../../musl-libc/src/string/wcpcpy.c"
#include "../../musl-libc/src/string/wcpncpy.c"
#include "../../musl-libc/src/string/wcscat.c"
#include "../../musl-libc/src/string/wcschr.c"
#include "../../musl-libc/src/string/wcscspn.c"
#include "../../musl-libc/src/string/wcsdup.c"
#include "../../musl-libc/src/string/wcsncat.c"
#include "../../musl-libc/src/string/wcspbrk.c"
#include "../../musl-libc/src/string/wcsrchr.c"
#include "../../musl-libc/src/string/wcsspn.c"
#include "../../musl-libc/src/string/wcstok.c"
#include "../../musl-libc/src/string/wcswcs.c"
#include "../../musl-libc/src/string/wmemchr.c"
#include "../../musl-libc/src/string/wmemcmp.c"
#include "../../musl-libc/src/string/wmemcpy.c"
#include "../../musl-libc/src/string/wmemmove.c"
#include "../../musl-libc/src/string/wmemset.c"
