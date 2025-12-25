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

#ifndef CORE_RUNTIME_PRIVATE_SPRTDSO_H_
#define CORE_RUNTIME_PRIVATE_SPRTDSO_H_

#include "SPRuntimeDso.h"

namespace sprt {

class SPRT_LOCAL Dso {
public:
	static constexpr const char *ERROR_MOVED_OUT = "Object was moved out";
	static constexpr const char *ERROR_NOT_LOADED = "Object was not loaded";

	~Dso() {
		if (_handle) {
			close();
		}
	}

	Dso() { }

	Dso(StringView name) : Dso(name, DsoFlags::Lazy) { }
	Dso(StringView name, DsoFlags flags) {
		flags &= DsoFlags::UserFlags;

		_handle = dso_open(name, flags, &_error);
		if (_handle) {
			_flags = flags;
		}
	}

	Dso(const char *name) : Dso(name, DsoFlags::Lazy) { }
	Dso(const char *name, DsoFlags flags) {
		flags &= DsoFlags::UserFlags;

		_handle = dso_open(name, flags, &_error);
		if (_handle) {
			_flags = flags;
		}
	}

	Dso(const Dso &) = delete;
	Dso &operator=(const Dso &) = delete;

	Dso(Dso &&other) {
		_flags = other._flags;
		_handle = other._handle;
		_error = other._error;

		other._flags = DsoFlags::None;
		other._handle = nullptr;
		other._error = ERROR_MOVED_OUT;
	}

	Dso &operator=(Dso &&other) {
		if (_handle) {
			close();
		}

		_flags = other._flags;
		_handle = other._handle;
		_error = other._error;

		other._flags = DsoFlags::None;
		other._handle = nullptr;
		other._error = ERROR_MOVED_OUT;
		return *this;
	}

	template <typename T = void *>
	T sym(StringView name, DsoSymFlags flags = DsoSymFlags::None) {
		return reinterpret_cast<T>(loadSym(name, flags));
	}

	template <typename T = void *>
	T sym(const char *name, DsoSymFlags flags = DsoSymFlags::None) {
		return reinterpret_cast<T>(loadSym(name, flags));
	}

	explicit operator bool() const { return _handle != nullptr; }

	DsoFlags getFlags() const { return _flags; }
	const char *getError() const { return _error; }

	void close() {
		if (_handle) {
			dso_close(_flags, _handle);
			_handle = nullptr;
			_flags = DsoFlags::None;
		} else {
			_error = ERROR_NOT_LOADED;
		}
	}

protected:
	void *loadSym(const char *name, DsoSymFlags flags) {
		if (_handle) {
			auto s = sprt::dso_sym(_handle, name, flags, &_error);
			if (s) {
				_error = nullptr;
				return s;
			}
		} else {
			_error = ERROR_NOT_LOADED;
		}
		return nullptr;
	}

	void *loadSym(StringView name, DsoSymFlags flags) {
		if (_handle) {
			auto s = sprt::dso_sym(_handle, name, flags, &_error);
			if (s) {
				_error = nullptr;
				return s;
			}
		} else {
			_error = ERROR_NOT_LOADED;
		}
		return nullptr;
	}

	DsoFlags _flags = DsoFlags::None;
	void *_handle = nullptr;
	const char *_error = nullptr;
};


} // namespace sprt

#endif // CORE_RUNTIME_PRIVATE_SPRTDSO_H_
