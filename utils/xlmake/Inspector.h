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

#ifndef UTILS_XLMAKE_INSPECTOR_H_
#define UTILS_XLMAKE_INSPECTOR_H_

#include "SPCommon.h"
#include "SPMakefile.h"

namespace xlmake {

using namespace sp;
using namespace sp::makefile;
using namespace sp::mem_pool;

// Options for the inspection mode (`xlmake -i ...`). All actions are combinable and act on
// the given targets, or the default goal when none are named.
struct InspectConfig {
	Vector<StringView> vars; // -V / --var (repeatable)
	Vector<StringView> targets; // positional goals
	bool printVars = false; // -p / --print-vars
	bool recipe = false; // --recipe
	bool prereqs = false; // --prerequisites
	bool recursive = false; // -r / --recursive (transitive closure for --prerequisites)
	bool outOfDate = false; // -q / --out-of-date
	bool phonyPrereqs = false; // -P / --phony-prereqs (judge phony by prerequisites, -r -q)
};

// Run the requested inspection actions against an already-loaded makefile. `makefilePaths` is
// used only for the no-action overview. Returns a process exit code (0 on success).
int runInspect(Makefile *mk, const InspectConfig &cfg, const Vector<String> &makefilePaths,
		ErrorReporter &err);

} // namespace xlmake

#endif /* UTILS_XLMAKE_INSPECTOR_H_ */
