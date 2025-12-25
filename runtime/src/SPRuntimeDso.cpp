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

#include "SPRuntimeDso.h"

#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS

#include <c/__sprt_dlfcn.h>

namespace sprt {

void *dso_open(StringView name, DsoFlags flags, const char **err) {
	void *h = nullptr;
	int f = 0;
	if ((flags & DsoFlags::Lazy) != DsoFlags::None) {
		f |= __SPRT_RTLD_LAZY;
	}
	if ((flags & DsoFlags::Global) != DsoFlags::None) {
		f |= __SPRT_RTLD_GLOBAL;
	}
	if ((flags & DsoFlags::Self) != DsoFlags::None) {
		h = ::__sprt_dlopen(nullptr, __SPRT_RTLD_LAZY);
	} else {
		name.performWithTerminated(
				[&](const char *ptr, size_t len) { h = ::__sprt_dlopen(ptr, f); });
	}
	if (!h) {
		*err = ::__sprt_dlerror();
	}
	return h;
}

void *dso_open(const char *name, DsoFlags flags, const char **err) {
	void *h = nullptr;
	int f = 0;
	if ((flags & DsoFlags::Lazy) != DsoFlags::None) {
		f |= __SPRT_RTLD_LAZY;
	}
	if ((flags & DsoFlags::Global) != DsoFlags::None) {
		f |= __SPRT_RTLD_GLOBAL;
	}
	if ((flags & DsoFlags::Self) != DsoFlags::None) {
		h = ::__sprt_dlopen(nullptr, __SPRT_RTLD_LAZY);
	} else {
		h = ::__sprt_dlopen(name, f);
	}
	if (!h) {
		*err = ::__sprt_dlerror();
	}
	return h;
}

void dso_close(DsoFlags flags, void *handle) {
	if (handle) {
		::__sprt_dlclose(handle);
	}
}

void *dso_sym(void *h, StringView name, DsoSymFlags flags, const char **err) {
	void *s = nullptr;
	name.performWithTerminated([&](const char *ptr, size_t len) { s = ::__sprt_dlsym(h, ptr); });
	if (!s) {
		*err = ::__sprt_dlerror();
	}
	return s;
}

void *dso_sym(void *h, const char *name, DsoSymFlags flags, const char **err) {
	auto s = ::__sprt_dlsym(h, name);
	if (!s) {
		*err = ::__sprt_dlerror();
	}
	return s;
}

} // namespace sprt

#endif

#if SPRT_WINDOWS

#include "provate/SPUnistd.h"
#include <libloaderapi.h>

namespace sprt {

static constexpr const char *WIN_FAIL_TO_LOAD = "Fail to load dynamic object";
static constexpr const char *WIN_SYMBOL_NOT_FOUND = "Fail to find symbol in dynamic object";

void *dso_open(StringView name, DsoFlags flags, const char **err) {
	HMODULE h = NULL;
	if ((flags & DsoFlags::Self) != DsoFlags::None) {
		h = GetModuleHandleA(nullptr);
	} else {
		name.performWithTerminated(
				[&](const char *ptr, size_t len) { h = LoadLibraryA(LPCSTR(ptr)); });
	}

	if (!h) {
		*err = WIN_FAIL_TO_LOAD;
	}
	return (void *)h;
}

void *dso_open(const char *name, DsoFlags flags, const char **err) {
	HMODULE h = NULL;
	if ((flags & DsoFlags::Self) != DsoFlags::None) {
		h = GetModuleHandleA(nullptr);
	} else {
		h = LoadLibraryA(LPCSTR(name));
	}

	if (!h) {
		*err = WIN_FAIL_TO_LOAD;
	}
	return (void *)h;
}

void dso_close(DsoFlags flags, void *handle) {
	if (handle) {
		if ((flags & DsoFlags::Self) == DsoFlags::None) {
			FreeLibrary(HMODULE(handle));
		}
	}
}

void *dso_sym(void *h, StringView name, DsoSymFlags flags, const char **err) {
	void *s = nullptr;
	name.performWithTerminated(
			[&](const char *ptr, size_t len) { s = GetProcAddress(HMODULE(h), ptr); });
	if (!s) {
		*err = WIN_SYMBOL_NOT_FOUND;
	}
	return (void *)s;
}

void *dso_sym(void *h, const char *name, DsoSymFlags flags, const char **err) {
	auto s = GetProcAddress(HMODULE(h), name);
	if (!s) {
		*err = WIN_SYMBOL_NOT_FOUND;
	}
	return (void *)s;
}

} // namespace sprt

#endif
