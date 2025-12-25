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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMEDSO_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMEDSO_H_

#include "SPRuntimeString.h"

namespace sprt {

enum class DsoFlags : uint32_t {
	None = 0,
	Self = 1
			<< 0, // open caller app itself instead of target library ( Dso(StringView(), DsoFlags::Self) )
	Lazy = 1 << 1, // use lazy binding if available (default)
	Global = 1 << 2,

	UserFlags = Self | Lazy | Global,

	StapplerAbi = 1
			<< 30, // set by implementation for Dso, opened with stappler-abi module instead of actual OS DSO
};

SPRT_DEFINE_ENUM_AS_MASK(DsoFlags)


enum class DsoSymFlags : uint32_t {
	None = 0,
	Executable = 1 << 0, // Symbol is executable
	Loader = 1 << 1, // Symbol is loader for other symbols
};

SPRT_DEFINE_ENUM_AS_MASK(DsoSymFlags)


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

} // namespace sprt

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMEDSO_H_
