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

// Internal markers that the predefined $(WRITE) / $(APPEND) variables expand to. A leading \x01
// (SOH) can never begin a real shell command, so a recipe line whose first token is one of these is
// unambiguously an in-process file-write directive: the executor performs it via Looper::writeFile
// (no child process) instead of spawning a shell. Defined once here and referenced by both the
// producer (setupStandardVariables in main.cpp, which assigns them to $(WRITE)/$(APPEND)) and the
// detector (Builder::parseWriteCommand in Executor.cpp), so the two can never drift apart.
static constexpr StringView WriteDirectiveMarker("\x01xlmake-write");
static constexpr StringView AppendDirectiveMarker("\x01xlmake-append");

// Markers for the immediate (synchronous, no child process) directives, performed in-process via the
// sp::filesystem API: $(MKDIR) (mkdir -p), $(REMOVE) (rm -rf), $(CP) (cp -f), $(ECHO) (print a line).
// Same \x01-sentinel scheme as WRITE/APPEND above.
static constexpr StringView MkdirDirectiveMarker("\x01xlmake-mkdir");
static constexpr StringView RemoveDirectiveMarker("\x01xlmake-remove");
static constexpr StringView CopyDirectiveMarker("\x01xlmake-copy");
static constexpr StringView EchoDirectiveMarker("\x01xlmake-echo");

// Options for the build/execution mode (the default mode; also `xlmake -b ...`).
struct BuildConfig {
	Vector<StringView> targets; // positional goals (default goal when empty)
	uint32_t jobs = 0; // -j N concurrent children; 0 => hardware_concurrency, JobsUnlimited => -j
	bool keepGoing = false; // -k / --keep-going
	bool dryRun = false; // -n / --dry-run
	bool silent = false; // -s / --silent

	// GNU make-compatible data-extraction flags (used by the VSCode Makefile Tools extension)
	bool printDatabase = false; // -p / --print-data-base: dump the makefile database (GNU -p)
	bool question = false; // -q / --question: run nothing; exit 1 if any target is out of date
	bool alwaysMake = false; // -B / --always-make: treat every target as out of date
	bool printDirectory = false; // emit Entering/Leaving directory lines (effective: -w, or sub-make)
	uint32_t makeLevel = 0; // recursion depth (GNU MAKELEVEL); shown as xlmake[N] in those lines
	StringView rootDir; // absolute working directory (after -C), for the directory lines

	// --no-space-escape: do NOT shell-escape a space inside a recipe path. By default xlmake turns a
	// path-internal space (the engine's PathSpacePlaceholder, e.g. from $< under a "My Src" tree) into
	// a form the shell keeps in one argument (POSIX `\ `, Windows `" "`), so an unquoted recipe Just
	// Works. With this flag the placeholder is decoded to a plain space and the recipe author is fully
	// responsible for quoting, matching GNU make's literal expansion.
	bool noSpaceEscape = false;
};

// Build the requested goals from an already-loaded makefile, running recipes as child processes
// multiplexed through a single-threaded dispatch event loop (no worker threads). Returns a process
// exit code (0 on success / up-to-date, non-zero on a failed build).
int runBuild(Makefile *mk, const BuildConfig &cfg, ErrorReporter &err);

} // namespace xlmake

#endif /* UTILS_XLMAKE_EXECUTOR_H_ */
