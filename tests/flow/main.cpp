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

// Sections of the stappler_flow suite, run in this order. With no section names every section
// runs; `--full` runs the complete sweeps, and `--emit=<dir>` rewrites the committed units:
// `flowtest --emit=tests/flow/gen flow-codegen` (or `--emit=tests/flow/codegen/gen codegen-emit`
// for the corpus), then rebuild.
struct TestEntry {
	sprt::StringView name;
	void (*fn)();
};

static const TestEntry s_testList[] = {
	{"flow-build", &stappler::performFlowBuildTests},
	{"flow-run", &stappler::performFlowRunTests},
	{"flow-scene", &stappler::performFlowSceneTests},
	{"flow-codegen", &stappler::performFlowCodegenTests},
	{"var-cast", &stappler::performVarCastTests},
	{"schema-layout", &stappler::performSchemaLayoutTests},
	{"schema-scalar", &stappler::performSchemaScalarTests},
	{"schema-blob", &stappler::performSchemaBlobTests},
	{"schema-spans", &stappler::performSchemaSpansTests},
	{"schema-migrate", &stappler::performSchemaMigrateTests},
	{"value-types32", &stappler::performValueTypes32Tests},
	{"schema-enums", &stappler::performSchemaEnumsTests},
	{"op-registry", &stappler::performOpRegistryTests},
	{"graph-locals", &stappler::performGraphLocalsTests},
	{"graph-asset", &stappler::performGraphAssetTests},
	{"graph-function-asset", &stappler::performGraphFunctionAssetTests},
	{"graph-validate", &stappler::performGraphValidateTests},
	{"graph-build", &stappler::performGraphBuildTests},
	{"graph-scope", &stappler::performGraphScopeTests},
	{"graph-scene", &stappler::performGraphSceneTests},
	{"parallel-build", &stappler::performParallelBuildTests},
	{"graph-enum-family", &stappler::performGraphEnumFamilyTests},
	{"graph-settings", &stappler::performGraphSettingsTests},
	{"graph-extensions", &stappler::performGraphExtensionsTests},
	{"ops-core", &stappler::performOpsCoreTests},
	{"ops-numeric", &stappler::performOpsNumericTests},
	{"parallel-classify", &stappler::performParallelClassifyTests},
	{"interp-local", &stappler::performInterpLocalTests},
	{"interp-dataflow", &stappler::performInterpDataflowTests},
	{"interp-exec", &stappler::performInterpExecTests},
	{"interp-stall", &stappler::performInterpStallTests},
	{"interp-deadlock", &stappler::performInterpDeadlockTests},
	{"interp-stepping", &stappler::performInterpSteppingTests},
	{"interp-loop", &stappler::performInterpLoopTests},
	{"interp-breakpoint", &stappler::performInterpBreakpointTests},
	{"interp-step-rollback", &stappler::performInterpStepRollbackTests},
	{"interp-oracle", &stappler::performInterpOracleTests},
	{"codegen-emit", &stappler::performCodegenEmitTests},
	{"codegen-synth", &stappler::performCodegenSynthTests},
	{"codegen-identity", &stappler::performCodegenIdentityTests},
	{"codegen-exact-arena", &stappler::performCodegenExactArenaTests},
	{"codegen-exact-fast", &stappler::performCodegenExactFastTests},
	{"codegen-ops", &stappler::performCodegenOpsTests},
};

int main(int argc, const char *argv[]) {
	return perform_main(argc, argv, [&]() -> int {
		auto isOption = [](sprt::StringView arg) {
			return arg == "--full" || arg == "--fast" || arg.starts_with("--emit=");
		};

		int sections = 0;
		for (int i = 1; i < argc; ++i) {
			auto arg = sprt::StringView(argv[i]);
			if (arg == "--full") {
				stappler::test::s_full = true;
			} else if (arg == "--fast") {
				stappler::test::s_full = false;
			} else if (arg.starts_with("--emit=")) {
				stappler::test::s_emitDir = StringView(arg.data() + 7, arg.size() - 7);
			} else {
				++sections;
			}
		}

		if (stappler::test::full()) {
			sprt::cout << "flowtest: FULL run\n";
		} else {
			sprt::cout << "flowtest: REDUCED run - sweeps are cut, pass --full for the gate\n";
		}

		if (sections == 0) {
			for (auto &it : s_testList) { it.fn(); }
		} else {
			for (int i = 1; i < argc; ++i) {
				auto arg = sprt::StringView(argv[i]);
				if (isOption(arg)) {
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

		auto failures = stappler::test::failures();
		sprt::cout << "\ntotal failures: " << failures
				   << (stappler::test::full() ? "  (full run)" : "  (REDUCED run - pass --full for the complete sweeps)")
				   << "\n";
		return failures > 254 ? 254 : failures;
	});
}
