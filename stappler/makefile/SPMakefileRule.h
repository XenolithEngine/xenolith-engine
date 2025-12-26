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

#ifndef CORE_MAKEFILE_SPMAKEFILERULE_H_
#define CORE_MAKEFILE_SPMAKEFILERULE_H_

#include "SPMakefileError.h"
#include "SPMakefileStmt.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

struct Prerequisite : AllocBase {
	StringView name;
	Prerequisite *next = nullptr;

	Prerequisite(StringView s) : name(s) { }
};

struct Rule : AllocBase {
	Stmt *rule = nullptr;
	Rule *next = nullptr;

	Rule(Stmt *s) : rule(s) { }
};

struct Target : AllocBase {
	StringView name;
	Prerequisite *prerequisitesList = nullptr;
	Prerequisite *prerequisitesTail = nullptr;
	Prerequisite *orderOnlyList = nullptr;
	Prerequisite *orderOnlyTail = nullptr;
	Rule *rulesList = nullptr;
	Rule *rulesTail = nullptr;

	Target(StringView s) : name(s) { }

	void addPrerequisite(StringView);
	void addOrderOnly(StringView);
	void addRule(Stmt *);

	bool isFile = true;
};

} // namespace stappler::makefile

#endif /* CORE_MAKEFILE_SPMAKEFILERULE_H_ */
