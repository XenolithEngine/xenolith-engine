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

#include "SPCommon.h"

#include <sprt/cxx/unordered_map>

#include "tests.h"

using namespace stappler;

// One entry per module test (defined in the matching per-topic subdirectory). Run all with no
// argument, or some of them by name: `stapplertest filesystem raster`.
//
// An array, not a hash map: the run order has to be the order written here. With an unordered_map
// the order is a function of the hash of the names, so adding one test silently reshuffles all the
// others - which turns any interaction between two of them into a bug that appears and disappears
// as unrelated tests are added.
struct TestEntry {
	sprt::StringView name;
	void (*fn)();
};

static const TestEntry s_testList[] = {
	{"makefile", &stappler::performMakefileTests},
	{"filesystem", &stappler::performFilesystemTests},
	{"fs-locations", &stappler::performFilesystemLocationTests},
	{"embedded", &stappler::performEmbeddedFilesystemTests},
	{"bidi", &stappler::performBidiTests},
	{"shape", &stappler::performShapeTests},
	{"glyph", &stappler::performGlyphTests},
	{"pug", &stappler::performPugTests},
	{"css", &stappler::performCssTests},
	{"css-flexgrid", &stappler::performFlexboxGridCssTests},
	{"css-table", &stappler::performTableCssTests},
	{"cmdline", &stappler::performCommandLineTests},
	{"raster", &stappler::performRasterTests},
	{"datavalue", &stappler::performDataValueTests},
	{"datamodel", &stappler::performDataModelTests},
	{"vg-stroke", &stappler::performVgStrokeTests},
	{"search-fuzzy", &stappler::performSearchFuzzyTests},
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
					}
				}
				if (!found) {
					sprt::cerr << "Test not found: " << argv[i] << "\n";
					return -1;
				}
				found->fn();
			}
		}

		sprt::cout << "\ntotal failures: " << stappler::test::failures() << "\n";
		return stappler::test::failures();
	});
}
