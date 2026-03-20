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

#include <sprt/runtime/dso.h>
#include "private/SPRTDso.h"

#include <sprt/c/__sprt_dlfcn.h>

namespace sprt {

static constexpr const char *ERROR_MOVED_OUT = "Object was moved out";
static constexpr const char *ERROR_NOT_LOADED = "Object was not loaded";

static thread_local uint32_t tl_dsoVersion = 0;

void _null_fn() { }

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

uint32_t Dso::GetCurrentVersion() { return tl_dsoVersion; }

Dso::~Dso() {
	if (_handle) {
		close();
	}
}

Dso::Dso() { }

Dso::Dso(StringView name, uint32_t v) : Dso(name, DsoFlags::Lazy, v) { }

Dso::Dso(StringView name, DsoFlags flags, uint32_t v) : _version(v) {
	flags &= DsoFlags::UserFlags;

	auto tmp = tl_dsoVersion;
	tl_dsoVersion = _version;
	_handle = sprt::dso_open(sprt::StringView(name.data(), name.size()), flags, &_error);
	if (_handle) {
		_flags = flags;
	}
	tl_dsoVersion = tmp;
}

Dso::Dso(Dso &&other) {
	_flags = other._flags;
	_handle = other._handle;
	_error = other._error;
	_version = other._version;

	other._flags = DsoFlags::None;
	other._handle = nullptr;
	other._error = ERROR_MOVED_OUT;
	other._version = 0;
}

Dso &Dso::operator=(Dso &&other) {
	if (_handle) {
		close();
	}

	_flags = other._flags;
	_handle = other._handle;
	_error = other._error;
	_version = other._version;

	other._flags = DsoFlags::None;
	other._handle = nullptr;
	other._error = ERROR_MOVED_OUT;
	other._version = 0;
	return *this;
}

void Dso::close() {
	if (_handle) {
		auto tmp = tl_dsoVersion;
		tl_dsoVersion = _version;
		sprt::dso_close(_flags, _handle);
		tl_dsoVersion = tmp;
		_handle = nullptr;
		_flags = DsoFlags::None;
	} else {
		_error = ERROR_NOT_LOADED;
	}
}

void *Dso::loadSym(StringView name, DsoSymFlags flags) {
	if (_handle) {
		auto tmp = tl_dsoVersion;
		tl_dsoVersion = _version;
		void *s = nullptr;
		s = sprt::dso_sym(_handle, sprt::StringView(name.data(), name.size()), flags, &_error);
		tl_dsoVersion = tmp;
		if (s) {
			_error = nullptr;
			return s;
		}
	} else {
		_error = ERROR_NOT_LOADED;
	}
	return nullptr;
}

} // namespace sprt
