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

#include <sprt/c/__sprt_fcntl.h>

#include "stdio.h"
#include "fcntl.h"
#include "unistd.h"
#include "string.h"
#include "io.h"

#include "specific.h"

static int __fmodeflags(const char *mode);

namespace sprt {

static FILE *__fopen(const char *__restrict filename, const char *__restrict mode, int shMode) {
	FILE *f;
	int fd;
	int flags;

	/* Check for valid initial mode character */
	if (!sprt::strchr("rwa", *mode)) {
		__sprt_errno = EINVAL;
		return 0;
	}

	/* Compute the flags to pass to open() */
	flags = __fmodeflags(mode);

	switch (shMode) {
	case _SH_DENYRW: flags |= __SPRT_O_SH_DENYRW; break;
	case _SH_DENYWR: flags |= __SPRT_O_SH_DENYWR; break;
	case _SH_DENYRD: flags |= __SPRT_O_SH_DENYRD; break;
	case _SH_DENYNO: flags |= __SPRT_O_SH_DENYNO; break;
	}

	fd = ::open64(filename, flags, 0666);
	if (fd < 0) {
		return 0;
	}
	if (flags & __SPRT_O_CLOEXEC) {
		fcntl(fd, __SPRT_F_SETFD, __SPRT_FD_CLOEXEC);
	}

	f = fdopen(fd, mode);
	if (f) {
		return f;
	}

	close(fd);
	return 0;
}

__SPRT_C_FUNC FILE *_fsopen(const char *__restrict filename, const char *__restrict mode,
		int sh) __SPRT_NOEXCEPT {
	FILE *ret = nullptr;
	platform::performWithNativePath(filename, [&](const char *nativePath) SPRT_LAMBDAINLINE {
		ret = __fopen(nativePath, mode, sh);
		return 0;
	}, 0);
	return ret;
}

__SPRT_C_FUNC FILE *fopen(const char *__restrict filename,
		const char *__restrict mode) __SPRT_NOEXCEPT {
	FILE *ret = nullptr;
	platform::performWithNativePath(filename, [&](const char *nativePath) SPRT_LAMBDAINLINE {
		ret = __fopen(nativePath, mode, _SH_DENYNO);
		return 0;
	}, 0);
	return ret;
}

__SPRT_C_FUNC int freopen_s(__SPRT_ID(FILE) * *stream, const char *fileName, const char *mode,
		__SPRT_ID(FILE) * oldStream) __SPRT_NOEXCEPT {
	if (!stream || !fileName || !mode || oldStream) {
		__sprt_errno = EINVAL;
		return EINVAL;
	}

	auto file = freopen(fileName, mode, oldStream);
	if (file) {
		*stream = file;
		return 0;
	}
	return __sprt_errno;
}

} // namespace sprt
