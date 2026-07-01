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

#ifndef CORE_MAKEFILE_SPMAKEFILEBUILDER_H_
#define CORE_MAKEFILE_SPMAKEFILEBUILDER_H_

#include "SPMakefile.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

// `jobs` sentinel for a bare `-j` (no number): no concurrency cap.
static constexpr uint32_t JobsUnlimited = maxOf<uint32_t>();

// The in-process directive markers behind $(WRITE)/$(APPEND)/$(MKDIR)/$(REMOVE)/$(CP)/$(ECHO) live in
// the engine (makefile::WriteDirectiveMarker …, SPMakefileDirectives.h): Makefile::init() assigns them
// (the producer), and the Builder recognizes their expanded values (the detector).

// Options for a parallel build (the engine's Looper-driven executor; also what `xlmake -b ...` uses).
struct SP_PUBLIC BuildConfig {
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

	// --no-space-escape: do NOT shell-escape a space inside a recipe path. By default the executor turns
	// a path-internal space (the engine's PathSpacePlaceholder, e.g. from $< under a "My Src" tree) into
	// a form the shell keeps in one argument (POSIX `\ `, Windows `" "`), so an unquoted recipe Just
	// Works. With this flag the placeholder is decoded to a plain space and the recipe author is fully
	// responsible for quoting, matching GNU make's literal expansion.
	bool noSpaceEscape = false;

	// Optional output sink: when set, ALL build output (progress, recipe/child output, errors, the
	// build-time summary) is delivered here as StringView chunks instead of sprt::cout/sprt::cerr. Lets
	// an embedding program (e.g. a live-reload app) capture the build log. Must outlive runBuild().
	const Callback<void(StringView)> *output = nullptr;

	// Optional `-p`/--print-data-base hook: called once (when `printDatabase` is set) to dump the
	// makefile database in GNU-make format. The dump itself is a CLI/inspection concern (xlmake), so the
	// engine only invokes this hook; a plain build leaves it null. Must outlive runBuild().
	const Callback<void()> *printDatabaseHook = nullptr;
};

// Build the requested goals from an already-loaded makefile, running recipes as child processes
// multiplexed through a single-threaded dispatch event loop (no worker threads). Acquires its own
// dispatch::Looper for the calling thread. Returns a process exit code (0 on success / up-to-date,
// non-zero on a failed build).
SP_PUBLIC int runBuild(Makefile *mk, const BuildConfig &cfg, ErrorReporter &err);

} // namespace stappler::makefile

#endif /* CORE_MAKEFILE_SPMAKEFILEBUILDER_H_ */
