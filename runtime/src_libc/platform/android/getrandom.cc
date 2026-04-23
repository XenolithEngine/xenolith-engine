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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_errno.h>
#include <sprt/cxx/detail/ctypes.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/sys/__sprt_random.h>

#include <unistd.h>
#include <stdlib.h>

#include <linux/unistd.h>

__SPRT_C_FUNC int __sprt_android_get_device_api_level();

namespace sprt {

static ssize_t getrandom(void *buf, size_t buflen, unsigned flags) {
	static int apiVersion = __sprt_android_get_device_api_level();
	long ret = -1;
	if (apiVersion >= 28) {
		ret = ::syscall(__NR_getrandom, buf, buflen, flags);
	}
	if (ret == -1) {
		int fd = __sprt_open("/dev/urandom", __SPRT_O_RDONLY);
		if (fd < 0) {
			return -1;
		}

		size_t total_read = 0;
		while (total_read < buflen) {
			ssize_t nread = __sprt_read(fd, (char *)buf + total_read, buflen - total_read);
			if (nread <= 0) {
				__sprt_close(fd);
				return -1;
			}
			total_read += nread;
		}

		__sprt_close(fd);
		return total_read;
	}
	return ret;
}

static int getentropy(void *buffer, size_t buffer_size) {
	if (buffer_size > 256) {
		__sprt_errno = EIO;
		return -1;
	}

	return getrandom(buffer, buffer_size, __SPRT_GRND_NONBLOCK);
}

} // namespace sprt
