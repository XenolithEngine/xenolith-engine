/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#include "SPMakefileRule.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

PatternInfo getPatternComponents(StringView str) {
	str.trimChars<StringView::WhiteSpace>();

	auto r = str.readUntil<StringView::Chars<'\\', '%'>>();
	if (str.is('%') || str.empty() || (str.is('\\') && str.size() == 1)) {
		// simple case - no escapes
		return PatternInfo{r, str.sub(1), str.is('%')};
	}

	BufferTemplate<Interface> buf;
	if (!r.empty()) {
		buf.put(r.data(), r.size());
	}

	do {
		if (str.is('\\')) {
			if (str.size() > 1) {
				buf.putc(str.at(1));
				str += 2;
			} else {
				buf.putc('\\');
			}
		}
		r = str.readUntil<StringView::Chars<'\\', '%'>>();
		if (!r.empty()) {
			buf.put(r.data(), r.size());
		}
	} while (!str.is('%') && !str.empty());

	if (str.is('%') && str.size() > 1) {
		return PatternInfo{buf.get().pdup(), str.sub(1), true};
	} else {
		return PatternInfo{buf.get().pdup(), StringView(), false};
	}
}

bool matchPattern(const PatternInfo &pattern, StringView word, StringView &stem) {
	if (pattern.isPattern) {
		bool start = pattern.start.empty() || word.starts_with(pattern.start);
		bool end = pattern.end.empty() || word.ends_with(pattern.end);
		if (start && end && word.size() >= pattern.start.size() + pattern.end.size()) {
			stem = word.sub(pattern.start.size(),
					word.size() - pattern.start.size() - pattern.end.size());
			return true;
		}
		return false;
	} else {
		if (word == pattern.start) {
			stem = StringView();
			return true;
		}
		return false;
	}
}

void Target::addPrerequisite(StringView str) {
	if (!prerequisitesTail) {
		prerequisitesTail = prerequisitesList = new (sprt::nothrow) Prerequisite(str.pdup());
	} else {
		prerequisitesTail->next = new (sprt::nothrow) Prerequisite(str.pdup());
		prerequisitesTail = prerequisitesTail->next;
	}
}

void Target::addOrderOnly(StringView str) {
	if (!orderOnlyTail) {
		orderOnlyTail = orderOnlyList = new (sprt::nothrow) Prerequisite(str.pdup());
	} else {
		orderOnlyTail->next = new (sprt::nothrow) Prerequisite(str.pdup());
		orderOnlyTail = orderOnlyTail->next;
	}
}

void Target::addRule(Stmt *stmt) {
	if (!rulesTail) {
		rulesTail = rulesList = new (sprt::nothrow) Rule(stmt);
	} else {
		rulesTail->next = new (sprt::nothrow) Rule(stmt);
		rulesTail = rulesTail->next;
	}
}

} // namespace stappler::makefile
