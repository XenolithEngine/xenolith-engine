/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_MAKEFILE_SPMAKEFILEERROR_H_
#define CORE_MAKEFILE_SPMAKEFILEERROR_H_

#include "SPMemory.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::makefile {

// This module uses mem_pool memory model
using namespace mem_pool;

struct Stmt;
struct Block;

struct LineOffset {
	StringView selected;
	uint32_t lineOffset = 0;
	uint32_t pos = 0;
};

struct FileLocation {
	StringView filename;
	StringView line;
	uint32_t lineno = 0;
	uint32_t pos = 0;

	LineOffset makeLineOffset();
	void describe(const Callback<void(StringView)> &);
};

struct ErrorReporter : FileLocation {
	using LogCallback = void (*)(void *, log::LogType, StringView);

	uint32_t lineSize = 1;

	uint32_t nerrors = 0;
	uint32_t nwarnings = 0;

	ErrorReporter *outer = nullptr;

	LogCallback callback = nullptr;
	void *ref;

	ErrorReporter(ErrorReporter *);
	ErrorReporter(const FileLocation &, ErrorReporter *);

	void setPos(const StringView &);

	void reportError(StringView, Stmt * = nullptr, Block * = nullptr, bool showSource = true);
	void reportWarning(StringView, Stmt * = nullptr, Block * = nullptr, bool showSource = true);
	void reportInfo(StringView, Stmt * = nullptr, Block * = nullptr, bool showSource = true);

	void report(log::LogType, StringView, Stmt * = nullptr, Block * = nullptr,
			bool showSource = true);

	void incrementErrors();
	void incrementWarnings();
};

} // namespace stappler::makefile

#endif /* CORE_MAKEFILE_SPMAKEFILEERROR_H_ */
