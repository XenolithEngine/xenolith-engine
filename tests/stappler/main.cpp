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
// argument, or a single one by name: `stapplertest filesystem`.
static sprt::__malloc_unordered_map<sprt::StringView, void (*)()> s_testList{
	{"makefile", &stappler::performMakefileTests},
	{"filesystem", &stappler::performFilesystemTests},
	{"bidi", &stappler::performBidiTests},
	{"shape", &stappler::performShapeTests},
	{"pug", &stappler::performPugTests},
	{"css", &stappler::performCssTests},
	{"css-flexgrid", &stappler::performFlexboxGridCssTests},
};

int main(int argc, const char *argv[]) {
	return perform_main(argc, argv, [&]() -> int {
		if (argc <= 1) {
			for (auto &it : s_testList) { it.second(); }
		} else {
			auto it = s_testList.find(argv[1]);
			if (it == s_testList.end()) {
				sprt::cerr << "Test not found: " << argv[1] << "\n";
				return -1;
			}
			it->second();
		}

		sprt::cout << "\ntotal failures: " << stappler::test::failures() << "\n";
		return stappler::test::failures();
	});
}
