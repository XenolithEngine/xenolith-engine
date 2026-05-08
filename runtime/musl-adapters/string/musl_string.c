#define __SPRT_BUILD

#include "../include/defs.h"

#if !SPRT_WINDOWS
#include "../../musl-libc/src/string/memcmp.c"
#endif

#pragma clang diagnostic ignored "-Wunused-label"
#pragma clang diagnostic ignored "-Wunused-variable"

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64
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
#include "../../musl-libc/src/string/strchr.c"
#include "../../musl-libc/src/string/strcmp.c"
#include "../../musl-libc/src/string/strcpy.c"
#include "../../musl-libc/src/string/strcspn.c"
#include "../../musl-libc/src/string/strdup.c"
#include "../../musl-libc/src/string/strerror_r.c"
#include "../../musl-libc/src/string/strlcat.c"
#include "../../musl-libc/src/string/strlcpy.c"
#include "../../musl-libc/src/string/strncasecmp.c"
#include "../../musl-libc/src/string/strncat.c"
#include "../../musl-libc/src/string/strncmp.c"
#include "../../musl-libc/src/string/strncpy.c"
#include "../../musl-libc/src/string/strndup.c"
#include "../../musl-libc/src/string/strnlen.c"
#include "../../musl-libc/src/string/strpbrk.c"
#include "../../musl-libc/src/string/strrchr.c"
#include "../../musl-libc/src/string/strsep.c"
#include "../../musl-libc/src/string/strspn.c"
#include "../../musl-libc/src/string/strstr.c"
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
