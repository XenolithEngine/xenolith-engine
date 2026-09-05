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
// Embox EL0 dynamic-loader shim.
//
// Decision D5: one statically linked ET_EXEC, no PT_INTERP, no ld.so. dlopen and
// dlsym therefore fail with a fixed message and dlclose/dladdr are no-ops.
//
// This is the milestone's posture rather than a permanent one -- a Vulkan ICD
// would need real dlopen -- but nothing on Embox needs it today, and a loader is
// a phase of its own.

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_dlfcn.h>

namespace sprt {

static const char *s_dlErr = nullptr;

} // namespace sprt

extern "C" {

void *dlopen(const char *path, int __flags) __SPRT_NOEXCEPT {
	(void)path;
	(void)__flags;
	sprt::s_dlErr = "dlopen: dynamic loading is not supported on Embox EL0";
	return nullptr;
}

void *dlsym(void *__SPRT_RESTRICT __handle, const char *__SPRT_RESTRICT __name) __SPRT_NOEXCEPT {
	(void)__handle;
	(void)__name;
	sprt::s_dlErr = "dlsym: dynamic loading is not supported on Embox EL0";
	return nullptr;
}

int dlclose(void *__handle) __SPRT_NOEXCEPT {
	(void)__handle;
	return 0;
}

char *dlerror(void) __SPRT_NOEXCEPT {
	const char *e = sprt::s_dlErr;
	sprt::s_dlErr = nullptr; // dlerror() clears the error after reporting it
	return (char *)e;
}

int dladdr(const void *__handle, __SPRT_ID(Dl_info) * __info) __SPRT_NOEXCEPT {
	(void)__handle;
	(void)__info;
	return 0; // 0 = not found (dladdr uses 0/non-0, not errno)
}

} // extern "C"
