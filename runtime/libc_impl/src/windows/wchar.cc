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

#include <stdlib.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>

#include "specific.h"

#include "../../include/__impl_file.h"

int __wfmodeflags(const wchar_t *mode);

namespace sprt {

static FILE *__wfopen(const wchar_t *__restrict filename, const wchar_t *__restrict wmode,
		int shMode) {
	FILE *f;
	int fd;
	int flags;

	/* Check for valid initial mode character */
	if (!wcschr(L"rwa", *wmode)) {
		__sprt_errno = EINVAL;
		return 0;
	}

	/* Compute the flags to pass to open() */
	flags = __wfmodeflags(wmode);

	switch (shMode) {
	case _SH_DENYRW: flags |= __SPRT_O_SH_DENYRW; break;
	case _SH_DENYWR: flags |= __SPRT_O_SH_DENYWR; break;
	case _SH_DENYRD: flags |= __SPRT_O_SH_DENYRD; break;
	case _SH_DENYNO: flags |= __SPRT_O_SH_DENYNO; break;
	}

	fd = ::_wopen(filename, flags, 0666);
	if (fd < 0) {
		return 0;
	}
	if (flags & __SPRT_O_CLOEXEC) {
		fcntl(fd, __SPRT_F_SETFD, __SPRT_FD_CLOEXEC);
	}

	auto modeLen = wcstombs(nullptr, wmode, 0) + 1;
	if (modeLen != (size_t)-1 && modeLen != (size_t)-2) {
		return nullptr;
	}

	auto mode = __sprt_typed_malloca(char, modeLen);
	wcstombs(mode, wmode, modeLen);

	f = fdopen(fd, mode);

	__sprt_freea(mode);

	if (f) {
		return f;
	}

	close(fd);
	return 0;
}

__SPRT_C_FUNC FILE *wfreopen(const wchar_t *__restrict filename, const wchar_t *__restrict mode,
		FILE *__restrict f) __SPRT_NOEXCEPT {
	int fl = __wfmodeflags(mode);
	FILE *f2;

	FLOCK(f);

	fflush(f);

	if (!filename) {
		if (fl & O_CLOEXEC) {
			fcntl(f->fd, F_SETFD, FD_CLOEXEC);
		}
		fl &= ~(O_CREAT | O_EXCL | O_CLOEXEC);
		if (fcntl(f->fd, F_SETFL, fl) < 0) {
			goto fail;
		}
	} else {
		f2 = _wfopen(filename, mode);
		if (!f2) {
			goto fail;
		}
		if (f2->fd == f->fd) {
			f2->fd = -1; /* avoid closing in fclose */
		} else if (dup3(f2->fd, f->fd, fl & O_CLOEXEC) < 0) {
			goto fail2;
		}

		f->flags = (f->flags & F_PERM) | f2->flags;
		f->read = f2->read;
		f->write = f2->write;
		f->seek = f2->seek;
		f->close = f2->close;

		fclose(f2);
	}

	f->mode = 0;
	f->locale = 0;
	FUNLOCK(f);
	return f;

fail2:
	fclose(f2);
fail:
	fclose(f);
	return NULL;
}

__SPRT_C_FUNC FILE *_wfsopen(const wchar_t *wpath, const wchar_t *wmode, int sh) __SPRT_NOEXCEPT {
	if (!wpath || !wmode) {
		__sprt_errno = EINVAL;
		return nullptr;
	}

	return platform::performWithNativePath(wpath, [&](const wchar_t *nativePath) SPRT_LAMBDAINLINE {
		return __wfopen(nativePath, wmode, sh);
	}, (FILE *)nullptr);
}

__SPRT_C_FUNC FILE *_wfopen(const wchar_t *wpath, const wchar_t *wmode) __SPRT_NOEXCEPT {
	return _wfsopen(wpath, wmode, _SH_DENYNO);
}

__SPRT_C_FUNC int _wfreopen_s(__SPRT_ID(FILE) * *stream, const wchar_t *fileName,
		const wchar_t *mode, __SPRT_ID(FILE) * oldStream) __SPRT_NOEXCEPT {
	if (!stream || !fileName || !mode || oldStream) {
		__sprt_errno = EINVAL;
		return EINVAL;
	}

	auto file = wfreopen(fileName, mode, oldStream);
	if (file) {
		*stream = file;
		return 0;
	}
	return __sprt_errno;
}

} // namespace sprt
