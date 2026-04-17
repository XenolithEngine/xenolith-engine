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

#include <sprt/runtime/utils/dso.h>

#define SPRT_DEFINE_PROTO(name) decltype(&::name) name = nullptr;
#define SPRT_LOAD_PROTO(handle, name) this->name = handle.sym<decltype(this->name)>(#name);

namespace sprt {

template <typename T>
static bool clearFunctionList(T first, T last) {
	while (first != last) {
		*first = nullptr;
		++first;
	}
	return true;
}

template <typename T>
static bool validateFunctionList(T first, T last) {
	while (first != last) {
		if (*first == nullptr) {
			clearFunctionList(first, last);
			return false;
		}
		++first;
	}
	return true;
}

} // namespace sprt

#endif // CORE_RUNTIME_PRIVATE_SPRTDSO_H_
