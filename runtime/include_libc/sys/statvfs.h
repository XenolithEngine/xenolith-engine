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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_STATVFS_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_STATVFS_H_

/*
	Dispatch header for the POSIX <sys/statvfs.h> (filesystem statistics):
	- hosted SPRT build -> forwards to the system <sys/statvfs.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	struct statvfs comes in via <sprt/c/sys/__sprt_statvfs.h> (bits/statvfs.h); the
	backing statvfs()/fstatvfs() are provided by libc_wrapper (SPRuntimeCSysStat.cpp),
	which forwards to the platform on hosted targets. Consumed by libc++'s <filesystem>
	support layer (std::filesystem::space).

	Macros:
	  mount flags:  ST_RDONLY, ST_NOSUID
	Types:
	  fsblkcnt_t, fsfilcnt_t
	Functions:
	  statvfs   - filesystem statistics by path
	  fstatvfs  - filesystem statistics by open descriptor
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/statvfs.h>

#else

#include <sprt/c/sys/__sprt_statvfs.h>

#ifndef ST_RDONLY
#define ST_RDONLY 1 /* read-only filesystem */
#define ST_NOSUID 2 /* setuid/setgid bits ignored */
#endif

typedef __SPRT_ID(fsblkcnt_t) fsblkcnt_t;
typedef __SPRT_ID(fsfilcnt_t) fsfilcnt_t;

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int statvfs(const char *__SPRT_RESTRICT __path,
		struct __SPRT_STATVFS_NAME *__SPRT_RESTRICT __buf) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_statvfs(__path, __buf);
}
#endif

SPRT_UMBRELLA_FUNC
int fstatvfs(int __fd, struct __SPRT_STATVFS_NAME *__buf) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fstatvfs(__fd, __buf);
}
#endif

__SPRT_END_DECL

#endif /* hosted dispatch */

#endif /* CORE_RUNTIME_INCLUDE_LIBC_SYS_STATVFS_H_ */
