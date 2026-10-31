/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef TESTS_BASIC3D_TESTS_H_
#define TESTS_BASIC3D_TESTS_H_

#include "SPCommon.h"
#include <sprt/runtime/stream.h>

namespace STAPPLER_VERSIONIZED stappler::test {

// The header-only harness of tests/uilayout: one failure counter for main() to turn into an exit
// code.
inline int s_failures = 0;

inline void check(bool cond, StringView name, StringView detail = StringView()) {
	sprt::cout << (cond ? "[ OK ] " : "[FAIL] ") << name;
	if (!cond && !detail.empty()) {
		sprt::cout << "  (" << detail << ")";
	}
	sprt::cout << "\n";
	if (!cond) {
		++s_failures;
	}
}

inline int failures() { return s_failures; }

inline bool near(float a, float b, float eps) {
	const float d = a - b;
	return (d < 0.0f ? -d : d) <= eps;
}

} // namespace stappler::test

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

void performCameraTests();
void performMeshTests();

} // namespace stappler::xenolith::basic3d

#endif /* TESTS_BASIC3D_TESTS_H_ */
