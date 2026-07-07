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

// WebAssembly uname backend.
//
// There is no host OS to query; report a fixed identity. A later milestone can
// replace the version/nodename fields with a host_info import derived from
// navigator.userAgent (see wasm-port-draft.adoc §3.7), cached once in wasm.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_utsname.h>

namespace sprt {

static void __wasm_uname_copy(char *dst, const char *src) {
	unsigned n = 0;
	while (src[n] && n < __SPRT_SYS_NAMELEN - 1) {
		dst[n] = src[n];
		++n;
	}
	dst[n] = 0;
}

__SPRT_C_FUNC int __SPRT_ID(uname)(struct __SPRT_UTSNAME_NAME *buf) {
	if (!buf) {
		return -1;
	}
	__builtin_memset(buf, 0, sizeof(struct __SPRT_UTSNAME_NAME));
	__wasm_uname_copy(buf->sysname, "WASM");
	__wasm_uname_copy(buf->nodename, "localhost");
	__wasm_uname_copy(buf->release, "1.0");
	__wasm_uname_copy(buf->version, "Stappler Runtime WebAssembly");
	__wasm_uname_copy(buf->machine, "wasm32");
	return 0;
}

} // namespace sprt
