/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include <c/__sprt_dlfcn.h>

#include "private/SPRTFilename.h"

#include <dlfcn.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(dlclose)(void *ptr) { return ::dlclose(ptr); }

__SPRT_C_FUNC char *__SPRT_ID(dlerror)(void) { return ::dlerror(); }

__SPRT_C_FUNC void *__SPRT_ID(dlopen)(const char *path, int __flags) {
	return internal::performWithNativePath(path,
			[&](const char *target) { return ::dlopen(target, __flags); }, (void *)nullptr);
}

__SPRT_C_FUNC void *__SPRT_ID(
		dlsym)(void *__SPRT_RESTRICT __handle, const char *__SPRT_RESTRICT __name) {
	return ::dlsym(__handle, __name);
}

__SPRT_C_FUNC int __SPRT_ID(dladdr)(const void *__handle, __SPRT_ID(Dl_info) * __info) {
	return ::dladdr(__handle, (::Dl_info *)__info);
}

} // namespace sprt
