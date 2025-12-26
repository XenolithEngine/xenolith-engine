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

#ifndef CORE_MAKEFILE_SPMAKEFILESTMT_H_
#define CORE_MAKEFILE_SPMAKEFILESTMT_H_

#include "SPMakefileError.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

struct Stmt;
struct StmtValue;

enum class Keyword {
	None,
	Include,
	IncludeOptional,
	Override,
	Define,
	Undefine,
	Ifdef,
	Ifndef,
	Ifeq,
	Ifneq,
	Else,
	Endif,
	Endef,
};

enum class StmtType {
	Word,
	WordList,
	ArgumentList,
	Expansion,
};

enum class ReadContext {
	LineStart,
	Expansion,
	LineEnd,
	Multiline,
	MultilineExpansion,
	ConditionalQuoted,
	ConditionalDoubleQuoted,
	PrerequisiteList,
	OrderOnlyList,
	TrailingRecipe,
};

enum class Origin {
	Undefined,
	Default,
	Automatic,
	Environment,
	File,
	EnvironmentOverride,
	CommandLine,
	Override
};

struct SP_PUBLIC StmtValue : AllocBase {
	union {
		Stmt *stmt;
		StringView str;
	};

	bool isStmt = true;
	StmtValue *next = nullptr;

	void set(StringView s) {
		str = s;
		isStmt = false;
	}

	void set(Stmt *s) {
		stmt = s;
		isStmt = true;
	}

	StmtValue() : stmt(nullptr), next(nullptr) { }
	StmtValue(StringView s) : str(s), isStmt(false), next(nullptr) { }
	StmtValue(Stmt *s) : stmt(s), isStmt(true), next(nullptr) { }
};

struct SP_PUBLIC Stmt : AllocBase {
	static Keyword getKeyword(StringView);
	static char getBeginChar(ReadContext);
	static char getEndChar(ReadContext);

	static StringView getOperator(StringView, bool allowRule);

	// note that we need at least 2 for a test
	static bool isWhitespace(const StringView &);
	static StringView skipWhitespace(StringView &);

	static StringView readLine(StringView &, ErrorReporter &err);

	static Stmt *readWord(StringView &str, ReadContext, ErrorReporter &err);

	static Stmt *readScoped(StringView &str, StmtType type, ReadContext, ErrorReporter &err);

	StmtType type = StmtType::Word;
	StmtValue *value = nullptr;
	StmtValue *tail = nullptr;
	FileLocation loc;

	Stmt(const FileLocation &);
	Stmt(const FileLocation &, StringView);
	Stmt(const FileLocation &, StmtType, StringView);
	Stmt(const FileLocation &, StmtType, Stmt *);
	Stmt(const FileLocation &, StmtType, StmtValue *, StmtValue *);

	StmtValue *add(StmtValue *val);
	StmtValue *add(StmtValue *val, StmtValue *last);

	void add(StringView);
	void add(Stmt *);

	void describe(const Callback<void(StringView)> &, uint32_t level = 0);
	void describe(uint32_t level = 0);
};

SP_PUBLIC StringView getOriginName(Origin);

SP_PUBLIC inline std::ostream &operator<<(std::ostream &stream, Origin o) {
	stream << getOriginName(o);
	return stream;
}

} // namespace stappler::makefile

#endif /* CORE_MAKEFILE_SPMAKEFILESTMT_H_ */
