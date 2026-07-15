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

#ifndef SPRT_WRAPPERS_WINDOWS_DIRECT_H_
#define SPRT_WRAPPERS_WINDOWS_DIRECT_H_

/*
	Substitute for MSVC's <direct.h> (directory-manipulation CRT extensions). The SPRT
	runtime has no separate directory-CRT; the _-prefixed MSVC spellings are thin
	forwarders over their POSIX equivalents (mkdir from <sys/stat.h>; chdir/rmdir/getcwd
	from <unistd.h>). Provided so Windows-targeting third-party C (e.g. llvm compiler-rt's
	InstrProfilingUtil.c) that includes <direct.h> for _mkdir resolves against SPRT.

	Public surface: _mkdir, _rmdir, _chdir, _getcwd. _mkdir drops the MSVC no-mode form
	onto POSIX mkdir with the usual 0777 (& umask) creation mode.
*/

#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int _mkdir(const char *__path) { return mkdir(__path, 0777); }
static inline int _rmdir(const char *__path) { return rmdir(__path); }
static inline int _chdir(const char *__path) { return chdir(__path); }
static inline char *_getcwd(char *__buf, int __size) {
	return getcwd(__buf, (__size < 0) ? (size_t)0 : (size_t)__size);
}

#ifdef __cplusplus
}
#endif

#endif // SPRT_WRAPPERS_WINDOWS_DIRECT_H_
