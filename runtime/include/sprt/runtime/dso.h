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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DSO_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DSO_H_

#include <sprt/runtime/enum.h>
#include <sprt/runtime/stringview.h>

namespace sprt {

enum class DsoFlags : uint32_t {
	None = 0,

	// open caller app itself instead of target library ( Dso(StringView(), DsoFlags::Self) )
	Self = 1 << 0,

	// use lazy binding if available (default)
	Lazy = 1 << 1,

	// link as RTLD_GLOBAL
	Global = 1 << 2,

	UserFlags = Self | Lazy | Global,

	// set by implementation for Dso, opened with stappler-abi module instead of actual OS DSO
	StapplerAbi = 1 << 30,
};

SPRT_DEFINE_ENUM_AS_MASK(DsoFlags)

enum class DsoSymFlags : uint32_t {
	None = 0,
	Executable = 1 << 0, // Symbol is executable
	Loader = 1 << 1, // Symbol is loader for other symbols
};

SPRT_DEFINE_ENUM_AS_MASK(DsoSymFlags)

// Do-nothing indicator function, do nothing but points to some location within runtime
SPRT_API void _null_fn();

SPRT_API void *dso_open(StringView name, DsoFlags flags, const char **err);

SPRT_API void *dso_open(const char *name, DsoFlags flags, const char **err);

SPRT_API void dso_close(DsoFlags flags, void *handle);

SPRT_API void *dso_sym(void *h, StringView name, DsoSymFlags flags = DsoSymFlags::None,
		const char **err = nullptr);

SPRT_API void *dso_sym(void *h, const char *name, DsoSymFlags flags = DsoSymFlags::None,
		const char **err = nullptr);

template <typename Type>
inline Type *dso_tsym(void *h, StringView name, DsoSymFlags flags = DsoSymFlags::None,
		const char **err = nullptr) {
	auto sym = dso_sym(h, name, flags, err);
	if (sym) {
		return reinterpret_cast<Type *>(sym);
	}
	return nullptr;
}

template <typename Type>
inline Type dso_tsym(void *h, const char *name, DsoSymFlags flags = DsoSymFlags::None,
		const char **err = nullptr) {
	auto sym = dso_sym(h, name, flags, err);
	if (sym) {
		return reinterpret_cast<Type *>(sym);
	}
	return nullptr;
}

class SPRT_API Dso {
public:
	// Version number for shared modules, defined, when DSO loaded
	// This is actual when called within some DSO operation (open/close/sym)
	static uint32_t GetCurrentVersion();

	~Dso();

	Dso();
	Dso(StringView, uint32_t v = 0); // Lazy | Local by default
	Dso(StringView, DsoFlags, uint32_t v = 0);

	Dso(const Dso &) = delete;
	Dso &operator=(const Dso &) = delete;

	Dso(Dso &&);
	Dso &operator=(Dso &&);

	template <typename T = void *>
	T sym(StringView name, DsoSymFlags flags = DsoSymFlags::None) {
		static_assert(sprt::is_pointer<T>::value, "Pointer type required to load from DSO");
		if constexpr (sprt::is_function_v<sprt::remove_pointer_t<T>>) {
			flags |= DsoSymFlags::Executable;
		}
		return reinterpret_cast<T>(loadSym(name, flags));
	}

	bool isValid() const { return _handle != nullptr; }

	explicit operator bool() const { return isValid(); }

	DsoFlags getFlags() const { return _flags; }
	StringView getError() const { return _error; }
	uint32_t getVersion() const { return _version; }

	void close();

protected:
	void *loadSym(StringView, DsoSymFlags);

	DsoFlags _flags = DsoFlags::None;
	void *_handle = nullptr;
	const char *_error = nullptr;
	uint32_t _version = 0;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DSO_H_
