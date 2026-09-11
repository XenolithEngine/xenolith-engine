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

// Tests for the localization module: string tables, the lookup chain, plural forms and tag
// substitution.
//
// Everything here runs with NO window, NO backend and NO scene, because `locale::` is a string table
// and a parser over it. The label half is exercised through `LabelBase`, which is a plain class: a
// Label node would need a Director, and what is asserted - that an explicit `setLocaleEnabled(false)`
// survives the next `setString` - is a property of the base.
//
// What is deliberately NOT here is anything that needs glyphs. Whether Persian actually JOINS is a
// question about HarfBuzz and a font with Arabic coverage, and it is answered by a screenshot rather
// than by a counter.

#include "SPCommon.h"

#include "tests.h"

using namespace stappler;

// One entry per topic. An array rather than a map so the run order is the order written here (see the
// same note in tests/stappler/main.cpp).
struct TestEntry {
	sprt::StringView name;
	void (*fn)();
};

static const TestEntry s_testList[] = {
	{"table", &stappler::xenolith::locale::performTableTests},
	{"fallback", &stappler::xenolith::locale::performFallbackTests},
	{"plural", &stappler::xenolith::locale::performPluralTests},
	{"tags", &stappler::xenolith::locale::performTagTests},
	{"label", &stappler::xenolith::locale::performLabelTests},
};

int main(int argc, const char *argv[]) {
	return perform_main(argc, argv, [&]() -> int {
		if (argc <= 1) {
			for (auto &it : s_testList) { it.fn(); }
		} else {
			for (int i = 1; i < argc; ++i) {
				const TestEntry *found = nullptr;
				for (auto &it : s_testList) {
					if (it.name == sprt::StringView(argv[i])) {
						found = &it;
						break;
					}
				}
				if (found) {
					found->fn();
				} else {
					sprt::cout << "unknown test: " << argv[i] << "\n";
					return 1;
				}
			}
		}
		auto failures = test::failures();
		sprt::cout << (failures ? "FAILED: " : "PASSED, failures: ") << failures << "\n";
		return failures == 0 ? 0 : 1;
	});
}
