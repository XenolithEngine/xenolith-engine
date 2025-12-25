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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMELOG_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMELOG_H_

#include "SPRuntimeString.h"
#include "SPRuntimeStream.h"

namespace sprt::log {

enum LogType {
	Verbose,
	Debug,
	Info,
	Warn,
	Error,
	Fatal,
	Default = Debug,
};

struct SPRT_API LogFeatures {
	static LogFeatures acquire();

	enum Features : uint32_t {
		None,
		AnsiCompatible = 1 << 0,
		Colors = 1 << 1,
		Bold = 1 << 2,
		Underline = 1 << 3,
		Italic = 1 << 4,
		Reverse = 1 << 5,
		Dim = 1 << 6,
	};

	Features features;
	uint32_t ncolors = 0;
	StringView drop;
	StringView bold;
	StringView underline;
	StringView italic;
	StringView reverse;
	StringView dim;

	StringView fblack;
	StringView fred;
	StringView fgreen;
	StringView fyellow;
	StringView fblue;
	StringView fmagenta;
	StringView fcyan;
	StringView fwhite;
	StringView fdef;

	StringView bblack;
	StringView bred;
	StringView bgreen;
	StringView byellow;
	StringView bblue;
	StringView bmagenta;
	StringView bcyan;
	StringView bwhite;
	StringView bdef;
};

SPRT_DEFINE_ENUM_AS_MASK(LogFeatures::Features)

struct SPRT_API SourceLocation {
	StringView fileName;
	StringView functionName;
	uint32_t fileLine = 0;
};

#define __SPRT_LOCATION ::sprt::log::SourceLocation{ __FILE__, __SPRT_FUNCTION__, __LINE__}

SPRT_API void print(LogType type, StringView prefix, StringView tag, StringView text);

template <typename... Args>
SPRT_API void vprint(LogType type, const SourceLocation &loc, StringView tag, Args &&...args) {
	LogFeatures features;
	size_t bufSize = 0;
	__processArgs<char>([&](StringView str) { bufSize += str.size(); }, forward<Args>(args)...);

	if (!loc.fileName.empty()) {
		features = LogFeatures::acquire();

		// add space for SourceLocation encoding
		bufSize += (1 + features.underline.size() + features.dim.size() + loc.fileName.size() + 1
				+ itoa(uint64_t(loc.fileLine), (char *)nullptr, 0) + features.drop.size());
	}

	bufSize += 1; // nullterm

	if (bufSize > 0) {
		auto buf = _makeCharBuffer(bufSize + 1);
		auto target = buf;
		auto targetSize = bufSize;
		__processArgs<char>([&](StringView str) {
			target = strappend(target, &targetSize, str.data(), str.size());
		}, forward<Args>(args)...);

		if (!loc.fileName.empty()) {
			target = strappend(target, &targetSize, " ", 1);
			target = strappend(target, &targetSize, features.underline.data(),
					features.underline.size());
			target = strappend(target, &targetSize, features.dim.data(), features.dim.size());
			target = strappend(target, &targetSize, loc.fileName.data(), loc.fileName.size());
			target = strappend(target, &targetSize, ":", 1);
			target = strappend(target, &targetSize, uint64_t(loc.fileLine));
			target = strappend(target, &targetSize, features.drop.data(), features.drop.size());
		}

		print(type, StringView(), tag, StringView(buf, bufSize - targetSize));
	}
}

} // namespace sprt::log

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMELOG_H_
