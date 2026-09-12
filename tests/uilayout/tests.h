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

#ifndef TESTS_UILAYOUT_TESTS_H_
#define TESTS_UILAYOUT_TESTS_H_

#include "SPCommon.h"
#include <sprt/runtime/stream.h>

namespace STAPPLER_VERSIONIZED stappler::test {

// The same header-only harness tests/stappler and tests/remote use: failures accumulate in one
// inline counter (a single instance across every translation unit) so main() can turn them into an
// exit code.
inline int s_failures = 0;

inline void check(bool cond, StringView name) {
	sprt::cout << (cond ? "[ OK ] " : "[FAIL] ") << name << "\n";
	if (!cond) {
		++s_failures;
	}
}

inline void checkEq(StringView got, StringView expect, StringView name) {
	bool ok = (got == expect);
	sprt::cout << (ok ? "[ OK ] " : "[FAIL] ") << name;
	if (!ok) {
		sprt::cout << "  (got \"" << got << "\", expected \"" << expect << "\")";
	}
	sprt::cout << "\n";
	if (!ok) {
		++s_failures;
	}
}

inline void checkEq(uint64_t got, uint64_t expect, StringView name) {
	bool ok = (got == expect);
	sprt::cout << (ok ? "[ OK ] " : "[FAIL] ") << name;
	if (!ok) {
		sprt::cout << "  (got " << got << ", expected " << expect << ")";
	}
	sprt::cout << "\n";
	if (!ok) {
		++s_failures;
	}
}

inline int failures() { return s_failures; }

// A float comparison with a tolerance, because a placement is arithmetic over sizes and gaps and
// the exact bits are not the claim. One point is far below anything a layout decision turns on.
inline void checkNear(float got, float expect, StringView name) {
	const bool ok = (got - expect) < 0.01f && (expect - got) < 0.01f;
	sprt::cout << (ok ? "[ OK ] " : "[FAIL] ") << name;
	if (!ok) {
		sprt::cout << "  (got " << got << ", expected " << expect << ")";
	}
	sprt::cout << "\n";
	if (!ok) {
		++s_failures;
	}
}

} // namespace stappler::test

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

void performFlexRtlTests();
void performLogicalBoxTests();
void performAlignKeywordTests();
void performGridTableRtlTests();

} // namespace stappler::xenolith::ui

#endif /* TESTS_UILAYOUT_TESTS_H_ */
