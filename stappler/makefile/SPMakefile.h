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

#ifndef CORE_MAKEFILE_SPMAKEFILE_H_
#define CORE_MAKEFILE_SPMAKEFILE_H_

#include "SPMakefileError.h"
#include "SPMakefileRule.h"
#include "SPMakefileVariable.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

class Makefile : public memory::PoolObject {
public:
	using PathCallback = Callback<void(StringView)>;
	using IncludeCallback = void (*)(void *, StringView path, const PathCallback &);
	using LogCallback = void (*)(void *, log::LogType, StringView);

	using PoolObject::PoolObject;

	virtual ~Makefile() = default;

	bool init();

	void setLogCallback(LogCallback, void * = nullptr);
	void setIncludeCallback(IncludeCallback, void * = nullptr);
	void setRootPath(StringView);

	bool include(StringView name, StringView data, bool copyData = true, ErrorReporter * = nullptr);
	bool include(const FileInfo &, ErrorReporter * = nullptr, bool optional = false);
	bool includeFileByPath(StringView, ErrorReporter * = nullptr, bool optional = false);

	const Variable *assignSimpleVariable(StringView, Origin, StringView, bool multiline = false);
	const Variable *assignRecursiveVariable(StringView, Origin, StringView, bool multiline = false);
	const Variable *appendToVariable(StringView, Origin, StringView, bool multiline = false);

	const Variable *assignSimpleVariable(StringView, Origin, StringView, ErrorReporter &,
			bool multiline = false);
	const Variable *assignRecursiveVariable(StringView, Origin, StringView, ErrorReporter &,
			bool multiline = false);
	const Variable *appendToVariable(StringView, Origin, StringView, ErrorReporter &,
			bool multiline = false);

	const Variable *getVariable(StringView) const;

	// content parsed as an included makefile
	// use $(print wordlist...) to output data
	bool eval(const Callback<void(StringView)> &, StringView name, StringView content);

	Target *addTarget(StringView name);
	bool addTargetPrerequisite(SpanView<Target *>, StringView decl, ErrorReporter &);

	bool undefineVariable(StringView, Origin, ErrorReporter &);

protected:
	bool processMakefileContent(StringView str, ErrorReporter &);
	bool processMakefileLine(StringView str, ErrorReporter &);

	bool processIfdefLine(StringView &str, bool negative, ErrorReporter &, Block *original);
	bool processIfeqLine(StringView &str, bool negative, ErrorReporter &, Block *original);
	bool processElseLine(StringView &str, ErrorReporter &);
	bool processEndifLine(StringView &str, ErrorReporter &);

	bool processDefineLine(StringView &str, ErrorReporter &);
	bool processDefineContentLine(StringView &str, Block *, ErrorReporter &);
	bool processEndefLine(StringView &str, ErrorReporter &);
	bool processUndefineLine(StringView &str, Origin varOrigin, ErrorReporter &);
	bool processSimpleLine(StringView &str, Origin varOrigin, ErrorReporter &);

	bool processIncludeLine(StringView &str, ErrorReporter &, bool optional);

	uint32_t _errors = 0;

	Vector<Target *> _currentTargets;
	Map<StringView, Target *> _targets;

	void *_logCallbackRef;
	LogCallback _logCallback = nullptr;

	void *_includeCallbackRef;
	IncludeCallback _includeCallback = nullptr;

	VariableEngine _engine;
};

using MakefileRef = SharedRef<Makefile>;

} // namespace stappler::makefile

#endif /* CORE_MAKEFILE_SPMAKEFILE_H_ */
