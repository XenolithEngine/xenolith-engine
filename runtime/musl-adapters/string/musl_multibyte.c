#include "../include/defs.h"

#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"

#include "../../musl-libc/src/multibyte/mbrlen.c"

#include "../../musl-libc/src/ctype/isascii.c"
#include "../../musl-libc/src/ctype/isalnum.c"
#include "../../musl-libc/src/ctype/isblank.c"
#include "../../musl-libc/src/ctype/iscntrl.c"
#include "../../musl-libc/src/ctype/isdigit.c"
#include "../../musl-libc/src/ctype/isgraph.c"
#include "../../musl-libc/src/ctype/islower.c"
#include "../../musl-libc/src/ctype/isprint.c"
#include "../../musl-libc/src/ctype/isspace.c"
#include "../../musl-libc/src/ctype/isupper.c"
#include "../../musl-libc/src/ctype/isxdigit.c"
#include "../../musl-libc/src/ctype/isalpha.c"
#include "../../musl-libc/src/ctype/ispunct.c"

#include "../../musl-libc/src/ctype/iswalnum.c"
#include "../../musl-libc/src/ctype/iswblank.c"
#include "../../musl-libc/src/ctype/iswcntrl.c"
#include "../../musl-libc/src/ctype/iswctype.c"
#include "../../musl-libc/src/ctype/iswdigit.c"
#include "../../musl-libc/src/ctype/iswgraph.c"
#include "../../musl-libc/src/ctype/iswlower.c"
#include "../../musl-libc/src/ctype/iswprint.c"
#include "../../musl-libc/src/ctype/iswspace.c"
#include "../../musl-libc/src/ctype/iswupper.c"
#include "../../musl-libc/src/ctype/iswxdigit.c"

#define table walpha_table
#include "../../musl-libc/src/ctype/iswalpha.c"
#undef table

#define table wpunct_table
#include "../../musl-libc/src/ctype/iswpunct.c"
#undef table
