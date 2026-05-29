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
#include <errno.h>
#include <fcntl.h>

#include <sprt/runtime/utils/base64.h>
#include <sprt/runtime/stringview.h>
#include <sprt/c/sys/__sprt_random.h>
#include <sys/stat.h>

namespace sprt {

bool __mktmppath(char *__itpl, size_t suffixLen, const Callback<bool(const char *, size_t)> &cb) {
	if (!__itpl) {
		errno = EINVAL;
		return false;
	}

	// check if template is valid
	StringView itpl(__itpl);
	if (itpl.sub(itpl.size() - suffixLen - 6, 6) != StringView("XXXXXX")) {
		__sprt_errno = EINVAL;
		return -1;
	}

	uint8_t randomBytes[6];
	char b64Bytes[8];
	int counter = 0;

	errno = 0;

	do {
		// File/directory creation in callback may fail with critical errno;
		// only fail with EEXIST is allowed to continue
		if (errno != EEXIST) {
			return -1;
		}

		// Generate random bytes for replacement string
		__sprt_getrandom(randomBytes, 6, __SPRT_GRND_RANDOM);

		// Use base64url to convert raw bytes into filepath string
		base64url::encode(randomBytes, 6, b64Bytes, 8);

		// Replace XXXXXX with new string
		memcpy(__itpl + itpl.size() - suffixLen - 6, b64Bytes, 6);

	} while (!cb(itpl.data(), itpl.size()) && ++counter < 64);

	return counter < 64;
}

__SPRT_C_FUNC int mkostemp(char *itpl, int _flags) __SPRT_NOEXCEPT {
	if ((_flags & ~(O_CREAT | O_RDWR | O_EXCL | O_APPEND | O_CLOEXEC | O_SYNC)) != 0) {
		errno = EINVAL;
		return -1;
	}

	int fd = -1;

	if (!__mktmppath(itpl, 0, [&](const char *path, size_t pathLength) {
		fd = ::open(path, O_CREAT | O_RDWR | O_EXCL | _flags, 0600);
		return fd >= 0;
	})) {
		return -1;
	}

	errno = 0;
	return fd;
}

__SPRT_C_FUNC int mkstemp(char *itpl) __SPRT_NOEXCEPT { return mkostemp(itpl, 0); }

__SPRT_C_FUNC char *mkdtemp(char *itpl) __SPRT_NOEXCEPT {
	if (!__mktmppath(itpl, 0, [&](const char *path, size_t pathLength) {
		return ::mkdir(path, 0600) == 0; //
	})) {
		return nullptr;
	}
	return itpl;
}

} // namespace sprt
