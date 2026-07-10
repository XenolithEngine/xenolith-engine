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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_REGEX_H_
#define CORE_RUNTIME_INCLUDE_LIBC_REGEX_H_

/*
	Dispatch header for the POSIX <regex.h> (POSIX regular expressions):
	- hosted SPRT build -> forwards to the system <regex.h> (#include_next)
	- otherwise         -> SPRT's own types + declarations, routed to __sprt_*
	                       on hosted consumers, or to the freestanding musl regex
	                       (plain symbols) on wasm/Windows.

	Types:  regex_t (opaque), regmatch_t { regoff_t rm_so, rm_eo }, regoff_t.
	Compile flags: REG_EXTENDED, REG_ICASE, REG_NEWLINE, REG_NOSUB.
	Exec flags:    REG_NOTBOL, REG_NOTEOL.
	Errors: REG_NOMATCH, REG_BADPAT, REG_ECOLLATE, REG_ECTYPE, REG_EESCAPE,
	        REG_ESUBREG, REG_EBRACK, REG_EPAREN, REG_EBRACE, REG_BADBR,
	        REG_ERANGE, REG_ESPACE, REG_BADRPT.
	Functions: regcomp, regexec, regerror, regfree.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <regex.h>

#else

#include <sprt/c/__sprt_regex.h>

typedef __SPRT_ID(regex_t) regex_t;
typedef __SPRT_ID(regmatch_t) regmatch_t;
typedef __SPRT_ID(regoff_t) regoff_t;

#define REG_EXTENDED __SPRT_REG_EXTENDED
#define REG_ICASE    __SPRT_REG_ICASE
#define REG_NEWLINE  __SPRT_REG_NEWLINE
#define REG_NOSUB    __SPRT_REG_NOSUB

#define REG_NOTBOL __SPRT_REG_NOTBOL
#define REG_NOTEOL __SPRT_REG_NOTEOL

#define REG_NOMATCH  __SPRT_REG_NOMATCH
#define REG_BADPAT   __SPRT_REG_BADPAT
#define REG_ECOLLATE __SPRT_REG_ECOLLATE
#define REG_ECTYPE   __SPRT_REG_ECTYPE
#define REG_EESCAPE  __SPRT_REG_EESCAPE
#define REG_ESUBREG  __SPRT_REG_ESUBREG
#define REG_EBRACK   __SPRT_REG_EBRACK
#define REG_EPAREN   __SPRT_REG_EPAREN
#define REG_EBRACE   __SPRT_REG_EBRACE
#define REG_BADBR    __SPRT_REG_BADBR
#define REG_ERANGE   __SPRT_REG_ERANGE
#define REG_ESPACE   __SPRT_REG_ESPACE
#define REG_BADRPT   __SPRT_REG_BADRPT

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int regcomp(regex_t *__preg, const char *__pattern, int __cflags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_regcomp(__preg, __pattern, __cflags);
}
#endif

SPRT_UMBRELLA_FUNC
int regexec(const regex_t *__preg, const char *__string, __SPRT_ID(size_t) __nmatch,
		regmatch_t *__pmatch, int __eflags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_regexec(__preg, __string, __nmatch, __pmatch, __eflags);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(size_t) regerror(int __errcode, const regex_t *__preg, char *__errbuf,
		__SPRT_ID(size_t) __errbuf_size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_regerror(__errcode, __preg, __errbuf, __errbuf_size);
}
#endif

SPRT_UMBRELLA_FUNC
void regfree(regex_t *__preg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_regfree(__preg);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_REGEX_H_
