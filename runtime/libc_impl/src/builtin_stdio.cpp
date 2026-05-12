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

#include <sprt/cxx/detail/ctypes.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_fcntl.h>

#include "../include/__impl_file.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

int __fmodeflags(const char *mode) {
	int flags;
	if (strchr(mode, '+')) {
		flags = __SPRT_O_RDWR;
	} else if (*mode == 'r') {
		flags = __SPRT_O_RDONLY;
	} else {
		flags = __SPRT_O_WRONLY;
	}
	if (strchr(mode, 'x')) {
		flags |= __SPRT_O_EXCL;
	}
	if (strchr(mode, 'e')) {
		flags |= __SPRT_O_CLOEXEC;
	}
	if (*mode != 'r') {
		flags |= __SPRT_O_CREAT;
	}
	if (*mode == 'w') {
		flags |= __SPRT_O_TRUNC;
	}
	if (*mode == 'a') {
		flags |= __SPRT_O_APPEND;
	}
	return flags;
}

#if SPRT_WINDOWS
#include "windows/file.cc"
#include "windows/stdio.cc"
#endif

#include "stdio/__overflow.cc"
#include "stdio/__stdio.cc"

#include "stdio/fflush.cc"
#include "stdio/fread.cc"
#include "stdio/fwrite.cc"
#include "stdio/perror.cc"
#include "stdio/ofl.cc"

#include "stdio/__toread.cc"
#include "stdio/__towrite.cc"

#include "stdio/freopen.cc"
#include "stdio/fclose.cc"
#include "stdio/ferror.cc"
#include "stdio/feof.cc"
#include "stdio/fput.cc"
#include "stdio/fget.cc"
#include "stdio/flockfile.cc"
#include "stdio/fseek.cc"

namespace sprt {
extern "C" {
ssize_t getdelim(char **__SPRT_RESTRICT __lineptr, size_t *__SPRT_RESTRICT __n, int __delim,
		FILE *__SPRT_RESTRICT __stream) __SPRT_NOEXCEPT {
	if (__lineptr == nullptr || __n == nullptr || __stream == nullptr
			|| (*__lineptr == nullptr && *__n != 0)) {
		__sprt_errno = EINVAL;
		return -1;
	}
	if (feof(__stream) || ferror(__stream)) {
		return -1;
	}

	if (*__lineptr == nullptr) {
		*__n = 256;
		*__lineptr = (char *)malloc(*__n);
		if (*__lineptr == nullptr) {
			//fseterr(__stream);
			__sprt_errno = ENOMEM;
			return -1;
		}
	}
	ssize_t nread = 0;
	int c = __SPRT_EOF;
	while (c != __delim) {
		c = fgetc(__stream);
		if (c == __SPRT_EOF) {
			break;
		}
		if (nread >= (ssize_t)(*__n - 1)) {
			size_t newn = *__n * 2;
			char *newptr = (char *)realloc(*__lineptr, newn);
			if (newptr == nullptr) {
				//fseterr(stream);
				__sprt_errno = ENOMEM;
				return -1;
			}
			*__lineptr = newptr;
			*__n = newn;
		}
		(*__lineptr)[nread++] = c;
	}
	if (c == __SPRT_EOF && nread == 0) {
		return -1;
	}
	(*__lineptr)[nread] = 0;
	return nread;
}

ssize_t getline(char **__SPRT_RESTRICT lineptr, size_t *__SPRT_RESTRICT n,
		FILE *__SPRT_RESTRICT stream) __SPRT_NOEXCEPT {
	return getdelim(lineptr, n, '\n', stream);
}
}
} // namespace sprt
