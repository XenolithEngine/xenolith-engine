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
	static constexpr int kMaxAttempts = 64;

	if (!__itpl) {
		errno = EINVAL;
		return false;
	}

	// Template must hold the "XXXXXX" placeholder immediately before the
	// suffix. Guard the length first: `size() - suffixLen - 6` underflows to a
	// huge value for short templates.
	StringView itpl(__itpl);
	if (itpl.size() < suffixLen + 6
			|| itpl.sub(itpl.size() - suffixLen - 6, 6) != StringView("XXXXXX")) {
		errno = EINVAL;
		return false;
	}

	const size_t offset = itpl.size() - suffixLen - 6;

	uint8_t randomBytes[6];
	char b64Bytes[8];

	for (int counter = 0; counter < kMaxAttempts; ++counter) {
		// Generate random bytes for the replacement string. Without checking
		// the result the buffer could stay uninitialized, yielding predictable
		// (or stack-garbage) names.
		if (__sprt_getrandom(randomBytes, sizeof(randomBytes), __SPRT_GRND_RANDOM)
				!= (__sprt_ssize_t)sizeof(randomBytes)) {
			errno = EIO;
			return false;
		}

		// Use base64url to convert raw bytes into a filepath-safe string; the
		// first 6 of the 8 emitted characters replace "XXXXXX".
		base64url::encode(randomBytes, sizeof(randomBytes), b64Bytes, sizeof(b64Bytes));
		memcpy(__itpl + offset, b64Bytes, 6);

		errno = 0;
		if (cb(itpl.data(), itpl.size())) {
			return true;
		}

		// Only a name collision (EEXIST) is retryable; any other failure from
		// the creation callback is fatal and is reported as-is.
		if (errno != EEXIST) {
			return false;
		}
	}

	// Exhausted all attempts without finding a free name.
	errno = EEXIST;
	return false;
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

#if defined(_WIN32)
// MSVC <io.h> _mktemp_s: fill the template's trailing "XXXXXX" with a name that does
// not currently exist and return 0; unlike mkstemp it does NOT create the file (the
// caller opens it, typically with _O_CREAT|_O_EXCL). Reuses the shared name generator.
__SPRT_C_FUNC int _mktemp_s(char *itpl, size_t size) __SPRT_NOEXCEPT {
	if (!itpl) {
		errno = EINVAL;
		return EINVAL;
	}
	// The name must be NUL-terminated within the declared buffer.
	StringView sv(itpl);
	if (sv.size() >= size) {
		errno = EINVAL;
		return EINVAL;
	}
	if (!__mktmppath(itpl, 0, [](const char *path, size_t) {
		struct stat st;
		if (::stat(path, &st) == 0) {
			errno = EEXIST; // name already taken -> retry
			return false;
		}
		return true; // free name found; leave the file uncreated
	})) {
		return errno ? errno : EEXIST;
	}
	errno = 0;
	return 0;
}
#endif // _WIN32

} // namespace sprt
