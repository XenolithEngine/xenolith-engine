/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

// musl's C string functions under __lfs_musl_* names: the renames apply to musl's
// own declarations too, so every call between them stays inside this set.

#define __SPRT_BUILD
#define _GNU_SOURCE

#define memcpy __lfs_musl_memcpy
#define memmove __lfs_musl_memmove
#define memset __lfs_musl_memset
#define memcmp __lfs_musl_memcmp
#define bcmp __lfs_musl_bcmp
#define memchr __lfs_musl_memchr
#define memrchr __lfs_musl_memrchr
#define __memrchr __lfs_musl___memrchr
#define strlen __lfs_musl_strlen
#define strnlen __lfs_musl_strnlen
#define strcmp __lfs_musl_strcmp
#define strncmp __lfs_musl_strncmp
#define strchr __lfs_musl_strchr
#define strchrnul __lfs_musl_strchrnul
#define __strchrnul __lfs_musl___strchrnul
#define strrchr __lfs_musl_strrchr

#include "defs.h"
#include <features.h>

// defs.h and musl's features.h stringify the alias target unexpanded; expand the renames first.
#undef weak_alias
#define __lfs_str(x) #x
#define __lfs_xstr(x) __lfs_str(x)
#define weak_alias(old, new) \
	extern __typeof(old) new __attribute__((__weak__, __alias__(__lfs_xstr(old))))

#include "../../../runtime/musl-libc/src/string/memcpy.c"
#undef LS
#undef RS
#include "../../../runtime/musl-libc/src/string/memmove.c"
#undef WS
#include "../../../runtime/musl-libc/src/string/memset.c"
#include "../../../runtime/musl-libc/src/string/memcmp.c"
#include "../../../runtime/musl-libc/src/string/bcmp.c"
#include "../../../runtime/musl-libc/src/string/memchr.c"
#undef SS
#undef ALIGN
#undef ONES
#undef HIGHS
#undef HASZERO
#include "../../../runtime/musl-libc/src/string/memrchr.c"
#include "../../../runtime/musl-libc/src/string/strlen.c"
#undef ALIGN
#undef ONES
#undef HIGHS
#undef HASZERO
#include "../../../runtime/musl-libc/src/string/strnlen.c"
#include "../../../runtime/musl-libc/src/string/strcmp.c"
#include "../../../runtime/musl-libc/src/string/strncmp.c"
#include "../../../runtime/musl-libc/src/string/strchrnul.c"
#include "../../../runtime/musl-libc/src/string/strchr.c"
#include "../../../runtime/musl-libc/src/string/strrchr.c"
