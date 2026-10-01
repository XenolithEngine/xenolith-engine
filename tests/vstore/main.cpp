/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "tests.h"

using namespace stappler;

// Sections of the stappler_vstore suite, run in this order. With no section names every section
// runs; `--full` runs the complete sweeps and composes with names in any order:
// `vstoretest --full journal-rollback`.
struct TestEntry {
	sprt::StringView name;
	void (*fn)();
};

static const TestEntry s_testList[] = {
	{"arena-basic", &stappler::performArenaBasicTests},
	{"arena-relocate", &stappler::performArenaRelocateTests},
	{"arena-torture", &stappler::performArenaTortureTests},
	{"arena-barrier", &stappler::performArenaBarrierTests},
	{"arena-oom", &stappler::performArenaOomTests},
	{"arena-header", &stappler::performArenaHeaderTests},
	{"arena-kinds", &stappler::performArenaKindsTests},
	{"arena-write-view", &stappler::performArenaWriteViewTests},
	{"journal-pages", &stappler::performJournalPagesTests},
	{"journal-rollback", &stappler::performJournalRollbackTests},
	{"journal-keyframe", &stappler::performJournalKeyframeTests},
	{"journal-compress", &stappler::performJournalCompressTests},
};

int main(int argc, const char *argv[]) {
	return perform_main(argc, argv, [&]() -> int {
		// Decided before the first section: a section reads it while choosing its arms.
		int sections = 0;
		for (int i = 1; i < argc; ++i) {
			auto arg = sprt::StringView(argv[i]);
			if (arg == "--full") {
				stappler::test::s_full = true;
			} else if (arg == "--fast") {
				stappler::test::s_full = false;
			} else {
				++sections;
			}
		}

		if (stappler::test::full()) {
			sprt::cout << "vstoretest: FULL run\n";
		} else {
			sprt::cout << "vstoretest: REDUCED run - sweeps and workloads are cut,"
						  " pass --full for the gate\n";
		}

		if (sections == 0) {
			for (auto &it : s_testList) { it.fn(); }
		} else {
			for (int i = 1; i < argc; ++i) {
				auto arg = sprt::StringView(argv[i]);
				if (arg == "--full" || arg == "--fast") {
					continue;
				}
				const TestEntry *found = nullptr;
				for (auto &it : s_testList) {
					if (it.name == arg) {
						found = &it;
					}
				}
				if (!found) {
					sprt::cerr << "Test not found: " << arg << "\n";
					return 1;
				}
				found->fn();
			}
		}

		auto f = stappler::test::failures();
		sprt::cout << "\ntotal failures: " << f;
		if (stappler::test::full()) {
			sprt::cout << "  (full run)\n";
		} else {
			sprt::cout << "  (REDUCED run - pass --full for the complete sweeps)\n";
		}

		// The exit code is a byte: an unclamped 256 failures would exit 0.
		return f > 254 ? 254 : f;
	});
}
