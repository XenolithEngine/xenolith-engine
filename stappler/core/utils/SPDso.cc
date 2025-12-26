/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#include "SPDso.h"
#include "SPMemory.h"

#ifdef MODULE_STAPPLER_ABI
#include "SPAbi.h"
#endif

namespace STAPPLER_VERSIONIZED stappler {

static constexpr const char *ERROR_MOVED_OUT = "Object was moved out";
static constexpr const char *ERROR_NOT_LOADED = "Object was not loaded";

static thread_local uint32_t tl_dsoVersion = 0;

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
#ifdef MODULE_STAPPLER_ABI
	// stappler-abi should work transparently for Dso invokation, so
	// try API first, if failed - try system DSO
	_handle = abi::open(name, flags, &_error);
	if (_handle) {
		flags |= DsoFlags::StapplerAbi;
	} else {
		_handle = dso::dso_open(name, flags, &_error);
	}
#else
	_handle = sprt::dso_open(sprt::StringView(name.data(), name.size()), flags, &_error);
#endif
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
#ifdef MODULE_STAPPLER_ABI
		if (hasFlag(_flags, DsoFlags::StapplerAbi)) {
			abi::close(_flags, _handle);
		} else {
			dso::dso_close(_flags, _handle);
		}
#else
		sprt::dso_close(_flags, _handle);
#endif
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
#ifdef MODULE_STAPPLER_ABI
		if (hasFlag(_flags, DsoFlags::StapplerAbi)) {
			s = abi::sym(_handle, name, flags, &_error);
		} else {
			s = dso::dso_sym(_handle, name, flags, &_error);
		}
#else
		s = sprt::dso_sym(_handle, sprt::StringView(name.data(), name.size()), flags, &_error);
#endif
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

} // namespace STAPPLER_VERSIONIZED stappler
