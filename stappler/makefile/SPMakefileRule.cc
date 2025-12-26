/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#include "SPMakefileRule.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

void Target::addPrerequisite(StringView str) {
	if (!prerequisitesTail) {
		prerequisitesTail = prerequisitesList = new (std::nothrow) Prerequisite(str.pdup());
	} else {
		prerequisitesTail->next = new (std::nothrow) Prerequisite(str.pdup());
		prerequisitesTail = prerequisitesTail->next;
	}
}

void Target::addOrderOnly(StringView str) {
	if (!orderOnlyTail) {
		orderOnlyTail = orderOnlyList = new (std::nothrow) Prerequisite(str.pdup());
	} else {
		orderOnlyTail->next = new (std::nothrow) Prerequisite(str.pdup());
		orderOnlyTail = orderOnlyTail->next;
	}
}

void Target::addRule(Stmt *stmt) {
	if (!rulesTail) {
		rulesTail = rulesList = new (std::nothrow) Rule(stmt);
	} else {
		rulesTail->next = new (std::nothrow) Rule(stmt);
		rulesTail = rulesTail->next;
	}
}

} // namespace stappler::makefile
