/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

// Integer formatting based on https://github.com/fmtlib/blob/master/fmt/include/fmt/format.h
// See original license https://github.com/fmtlib/fmt/blob/master/LICENSE
// Only base function and table data used

// Double formatting from https://github.com/miloyip/dtoa-benchmark/tree/master
// See original license https://github.com/miloyip/dtoa-benchmark/blob/master/license.txt
// Implemented buffer usage counter and support for different char types

#include "SPString.h"
#include "SPStringDetail.h"

#if LINUX || ANDROID || MACOS
#include <cxxabi.h>
#endif

namespace STAPPLER_VERSIONIZED stappler::string::detail {

template <typename Stream>
static void printDemangled(const Stream &stream, const std::type_info &t) {
#if LINUX || ANDROID || MACOS
	int status = 0;
	auto name = abi::__cxa_demangle(t.name(), nullptr, nullptr, &status);
	if (status == 0) {
		streamWrite(stream, name);
		::free(name);
	} else {
		streamWrite(stream, t.name());
	}
#else
	streamWrite(stream, t.name());
#endif
}

void streamWrite(const Callback<void(WideStringView)> &stream, const StringView &c) {
	auto len = sprt::unicode::getUtf16Length(c);
	char16_t buf[len + 1];
	sprt::unicode::toUtf16(buf, len, c);

	streamWrite(stream, buf);
}

void streamWrite(const std::function<void(WideStringView)> &stream, const StringView &c) {
	auto len = sprt::unicode::getUtf16Length(c);
	char16_t buf[len + 1];
	sprt::unicode::toUtf16(buf, len, c);

	streamWrite(stream, buf);
}

void streamWrite(const memory::function<void(WideStringView)> &stream, const StringView &c) {
	auto len = sprt::unicode::getUtf16Length(c);
	char16_t buf[len + 1];
	sprt::unicode::toUtf16(buf, len, c);

	streamWrite(stream, buf);
}

void streamWrite(const Callback<void(StringView)> &stream, const std::type_info &c) {
	printDemangled(stream, c);
}
void streamWrite(const Callback<void(WideStringView)> &stream, const std::type_info &c) {
	printDemangled(stream, c);
}
void streamWrite(const Callback<void(StringViewUtf8)> &stream, const std::type_info &c) {
	printDemangled(stream, c);
}
void streamWrite(const std::function<void(StringView)> &stream, const std::type_info &c) {
	printDemangled(stream, c);
}
void streamWrite(const std::function<void(WideStringView)> &stream, const std::type_info &c) {
	printDemangled(stream, c);
}
void streamWrite(const std::function<void(StringViewUtf8)> &stream, const std::type_info &c) {
	printDemangled(stream, c);
}
void streamWrite(const memory::function<void(StringView)> &stream, const std::type_info &c) {
	printDemangled(stream, c);
}
void streamWrite(const memory::function<void(WideStringView)> &stream, const std::type_info &c) {
	printDemangled(stream, c);
}
void streamWrite(const memory::function<void(StringViewUtf8)> &stream, const std::type_info &c) {
	printDemangled(stream, c);
}

} // namespace stappler::string::detail
