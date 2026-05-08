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

#ifndef RUNTIME_FREESTANDING_SRC_WINDOWS_DLLTABLE_H_
#define RUNTIME_FREESTANDING_SRC_WINDOWS_DLLTABLE_H_

#include <sprt/cxx/__functional/invoke.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/wrappers/windows/basic_types.h>

#define SPWIN_DEFINE_PROTO(Name) DllTableRecord Name = DllTableRecord{#Name};

namespace sprt {

struct DllTableRecord {
	const char *name = nullptr;
	FARPROC fn = nullptr;
};

struct DllTable {
	const wchar_t *__name = nullptr;
	DllTableRecord *__start = nullptr;
	DllTableRecord *__end = nullptr;
	DllTableRecord *__preloads = nullptr;
	HANDLE __handle = nullptr;
	bool __debug = false;

	DllTable(const wchar_t *, DllTableRecord *start, DllTableRecord *end,
			DllTableRecord *preloads = nullptr);

	bool init(HANDLE);

	bool load(DllTableRecord *);

	template <typename FnPointerType, typename... Args>
	inline auto call(DllTableRecord &rec, Args &&...args) {
		if (!rec.fn) {
			if (!load(&rec)) {
				__sprt_abort();
			}
		}
		return sprt::__invoke(reinterpret_cast<FnPointerType>(rec.fn),
				sprt::forward<Args>(args)...);
	}
};

} // namespace sprt

#endif // RUNTIME_FREESTANDING_SRC_WINDOWS_DLLTABLE_H_
