/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "SPCommon.h"

#include "tests.h"

using namespace stappler;

struct TestEntry {
	sprt::StringView name;
	void (*fn)();
};

static const TestEntry s_testList[] = {
	{"camera", &stappler::xenolith::basic3d::performCameraTests},
	{"mesh", &stappler::xenolith::basic3d::performMeshTests},
	{"material", &stappler::xenolith::basic3d::performMaterialTests},
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
