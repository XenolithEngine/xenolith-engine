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

#ifndef TESTS_FLOW_TESTS_H_
#define TESTS_FLOW_TESTS_H_

#include "SPCommon.h"
#include "SPFlowInterp.h"

#include <sprt/runtime/stream.h>

namespace STAPPLER_VERSIONIZED stappler::test {

// Same assertions as tests/vstore: failures accumulate in one counter and main() returns it.
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

inline int failures() { return s_failures; }

// `--full` is the gate: every sweep at full size. Without it a section runs its reduced arm - the
// same code and assertions over fewer arms.
inline bool s_full = false;

inline bool full() { return s_full; }

inline uint32_t sized(uint32_t fullValue, uint32_t reducedValue) {
	return s_full ? fullValue : reducedValue;
}

// `--emit=<dir>`: the sections that own a committed unit write it there instead of only checking it.
inline StringView s_emitDir;

} // namespace stappler::test

namespace STAPPLER_VERSIONIZED stappler {

// The kind most sections run on: it announces its writes and keeps a shadow copy to prove the
// barrier had no holes. In a release build it is TrackedArena. A section about the kinds names them
// directly. Runs are in the kernel's own environment, with no scene.
using Arena = flow::value::ShadowArena;
using LocalStore = flow::LocalStoreT<Arena, flow::RuntimeGraph, flow::NoEnv>;
using Interpreter = flow::InterpreterT<Arena, flow::NoEnv>;
using RunConfig = flow::RunConfigT<Arena, flow::NoEnv>;

void performFlowBuildTests();
void performFlowRunTests();
void performFlowSceneTests();
void performFlowCodegenTests();

void performVarCastTests();
void performSchemaLayoutTests();
void performSchemaScalarTests();
void performSchemaBlobTests();
void performSchemaSpansTests();
void performSchemaMigrateTests();
void performValueTypes32Tests();
void performSchemaEnumsTests();
void performOpRegistryTests();
void performGraphLocalsTests();
void performGraphAssetTests();
void performGraphValidateTests();
void performGraphBuildTests();
void performGraphScopeTests();
void performGraphSceneTests();
void performParallelBuildTests();
void performGraphEnumFamilyTests();
void performGraphSettingsTests();
void performGraphExtensionsTests();
void performOpsCoreTests();
void performOpsNumericTests();
void performParallelClassifyTests();
void performInterpLocalTests();
void performInterpDataflowTests();
void performInterpExecTests();
void performInterpStallTests();
void performInterpDeadlockTests();
void performInterpSteppingTests();
void performInterpLoopTests();
void performInterpBreakpointTests();
void performInterpStepRollbackTests();
void performInterpOracleTests();
void performCodegenEmitTests();
void performCodegenSynthTests();
void performCodegenIdentityTests();
void performCodegenExactArenaTests();
void performCodegenExactFastTests();
void performCodegenOpsTests();
} // namespace stappler

#endif /* TESTS_FLOW_TESTS_H_ */
