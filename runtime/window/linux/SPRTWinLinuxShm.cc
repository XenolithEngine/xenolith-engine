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

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

namespace sprt::window {

int createAnonymousFile(size_t size) {
	static const char tpl[] = "/xl-shm-XXXXXX";
	const char *path;
	int fd;
	int ret;

	// Sealed against shrinking, so a compositor that maps the whole pool cannot be handed a file
	// that later gets truncated under it.
	fd = ::memfd_create("xl-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (fd >= 0) {
		::fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_SEAL);
	} else {
		path = getenv("XDG_RUNTIME_DIR");
		if (!path) {
			errno = ENOENT;
			return -1;
		}

		char *tmpname = (char *)::calloc(strlen(path) + sizeof(tpl), 1);
		::strcpy(tmpname, path);
		::strcat(tmpname, tpl);

		fd = ::mkostemp(tmpname, O_CLOEXEC);
		if (fd >= 0) {
			::unlink(tmpname);
			::free(tmpname);
		} else {
			::free(tmpname);
			return -1;
		}
	}

	// Allocate up front rather than relying on sparse growth: a SIGBUS on first touch of a page
	// the filesystem could not back would land in the middle of rasterization.
	ret = ::posix_fallocate(fd, 0, off_t(size));
	if (ret != 0) {
		::close(fd);
		errno = ret;
		return -1;
	}
	return fd;
}

} // namespace sprt::window
