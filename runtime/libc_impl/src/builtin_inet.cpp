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

#include <arpa/inet.h>

extern "C" {

in_addr_t inet_network(const char *p) SPRT_NOEXCEPT { return ntohl(inet_addr(p)); }

struct in_addr inet_makeaddr(in_addr_t n, in_addr_t h) SPRT_NOEXCEPT {
	if (n < 256) {
		h |= n << 24;
	} else if (n < 65'536) {
		h |= n << 16;
	} else {
		h |= n << 8;
	}
	return (struct in_addr){.S_un{.S_addr = h}};
}

in_addr_t inet_lnaof(struct in_addr in) SPRT_NOEXCEPT {
	uint32_t h = in.s_addr;
	if (h >> 24 < 128) {
		return h & 0xff'ffff;
	}
	if (h >> 24 < 192) {
		return h & 0xffff;
	}
	return h & 0xff;
}

in_addr_t inet_netof(struct in_addr in) SPRT_NOEXCEPT {
	uint32_t h = in.s_addr;
	if (h >> 24 < 128) {
		return h >> 24;
	}
	if (h >> 24 < 192) {
		return h >> 16;
	}
	return h >> 8;
}
}
