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

#ifndef TESTS_LIBC_C_TEST_UTIL_H
#define TESTS_LIBC_C_TEST_UTIL_H

#include <stdio.h>
#include <stddef.h>
#include <errno.h>

namespace sprt::test {

// Map the errno values that the file/IO tests can produce to a stable symbolic
// name, so the host (glibc) and the freestanding libc_impl (whose numeric errno
// values may differ) diff identically. Unhandled values fall back to "E<num>".
static inline const char *errnoName(int e) {
	switch (e) {
		case 0: return "0";
		case ENOENT: return "ENOENT";
		case EEXIST: return "EEXIST";
		case EACCES: return "EACCES";
		case EINVAL: return "EINVAL";
		case EBADF: return "EBADF";
		case EISDIR: return "EISDIR";
		case ENOTDIR: return "ENOTDIR";
		case ERANGE: return "ERANGE";
		case ESPIPE: return "ESPIPE";
		case EROFS: return "EROFS";
		default: break;
	}
	static char buf[16];
	snprintf(buf, sizeof(buf), "E%d", e);
	return buf;
}

// The C standard only fixes the SIGN of comparison-function results, not their
// magnitude. glibc and musl legitimately differ in magnitude, so identity is
// checked on the sign alone.
static inline int sgn(long v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

// Print an offset of a result pointer within a base buffer, or "NULL" / "end".
// Avoids printing raw addresses (which differ between runs/platforms).
static inline void printOffset(const char *label, const void *base, const void *p,
		size_t len) {
	if (!p) {
		printf("%s=NULL\n", label);
	} else {
		auto off = (const char *)p - (const char *)base;
		if ((size_t)off == len) {
			printf("%s=end(%zd)\n", label, off);
		} else {
			printf("%s=%zd\n", label, off);
		}
	}
}

// Deterministic hex dump of a buffer.
static inline void printHex(const char *label, const void *buf, size_t len) {
	auto p = (const unsigned char *)buf;
	printf("%s=", label);
	for (size_t i = 0; i < len; ++i) { printf("%02x", p[i]); }
	printf("\n");
}

} // namespace sprt::test

#endif // TESTS_LIBC_C_TEST_UTIL_H
