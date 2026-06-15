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

#ifndef UTILS_XLMAKE_EXECUTOR_H_
#define UTILS_XLMAKE_EXECUTOR_H_

#include "SPCommon.h"
#include "SPMakefile.h"

namespace xlmake {

using namespace sp;
using namespace sp::makefile;
using namespace sp::mem_pool;

// `jobs` sentinel for a bare `-j` (no number): no concurrency cap.
static constexpr uint32_t JobsUnlimited = maxOf<uint32_t>();

// Options for the build/execution mode (`xlmake -b ...`).
struct BuildConfig {
	Vector<StringView> targets; // positional goals (default goal when empty)
	uint32_t jobs = 0; // -j N concurrent children; 0 => hardware_concurrency, JobsUnlimited => -j
	bool keepGoing = false; // -k / --keep-going
	bool dryRun = false; // -n / --dry-run
	bool silent = false; // -s / --silent
};

// Build the requested goals from an already-loaded makefile, running recipes as child processes
// multiplexed through a single-threaded dispatch event loop (no worker threads). Returns a process
// exit code (0 on success / up-to-date, non-zero on a failed build).
int runBuild(Makefile *mk, const BuildConfig &cfg, ErrorReporter &err);

} // namespace xlmake

#endif /* UTILS_XLMAKE_EXECUTOR_H_ */
