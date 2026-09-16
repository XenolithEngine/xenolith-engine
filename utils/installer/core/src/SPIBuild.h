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

#ifndef UTILS_INSTALLER_CORE_SRC_SPIBUILD_H_
#define UTILS_INSTALLER_CORE_SRC_SPIBUILD_H_

#include "SPICommon.h"
#include "SPIDirs.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

struct SP_PUBLIC BuildOptions {
	String target; // empty → native host triple
	bool run = false; // run the freshly-built binary afterwards (native builds only)
	bool release = false;
	uint32_t jobs = 0; // 0 → hardware concurrency
};

struct SP_PUBLIC BuildResult : OperationResult {
	String message; // human-readable summary
	String executable; // the produced binary, "" when the build produced none
	int exitCode = 0; // the build's exit code (0 ok, 2 build failure)
	int runExitCode = -1; // exit code of the --run child; -1 when it was not run
};

// Build the project at `path` (which must contain a Makefile) with the SDK toolchain, driving the
// engine's own make implementation (stappler_makefile) in-process — no external `make` is spawned.
//
// The build runs on a private job thread (see SPIJob.h): makefile execution needs a Looper of its
// own, and the job thread is also the only place that touches the process-global cwd and
// environment. Cross targets (target base != host) build the `install` goal; native builds may
// `--run` the result. `output` (optional) receives the build's output as it is produced, ON THE JOB
// THREAD, and must outlive the call.
SP_PUBLIC BuildResult buildProject(StringView path, const Layout &layout, const BuildOptions &opts,
		StringView engineOverride = StringView(),
		const Callback<void(StringView)> *output = nullptr);

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPIBUILD_H_
