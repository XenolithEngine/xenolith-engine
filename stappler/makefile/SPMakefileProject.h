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

#ifndef CORE_MAKEFILE_SPMAKEFILEPROJECT_H_
#define CORE_MAKEFILE_SPMAKEFILEPROJECT_H_

#include "SPMakefile.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

// GNU make's runner-agnostic standard predefined variables (Origin::Default, overridable): the
// toolchain program names, the compile/link recipe templates, the recipe/shell specials, the suffix
// list, and the per-invocation MAKE/CURDIR/MAKECMDGOALS. The xlmake identity (XLMAKE_VERSION, host
// vars, $(WRITE)/$(MKDIR)/... markers) is NOT set here — it is intrinsic to every Makefile via
// Makefile::init(). `rootDir` becomes $(CURDIR). Shared by the loadProject() loader and by xlmake.
SP_PUBLIC void setupStandardVariables(Makefile *, StringView rootDir, ErrorReporter &);

// Load a Stappler/GNU-make project's compile graph for the current host target, ready for read-only
// introspection (e.g. Makefile::getSourceInputs / SourceObserver). Creates a Makefile (which
// auto-carries the xlmake identity from init(), so the Stappler build takes its `init-xlmake.mk`
// path), applies setupStandardVariables, sets STAPPLER_BUILD=1 (so universal.mk expands the real
// source graph rather than its recursive launcher), and includes the project's
// GNUmakefile/makefile/Makefile from `projectDir` by absolute path. Returns null if none is found.
//
// Trust model (see SPMakefile.h): loading fully evaluates the makefile, including $(shell) and the
// build's configuration side effects — point it only at trusted project directories.
SP_PUBLIC Rc<MakefileRef> loadProject(StringView projectDir, ErrorReporter &);

} // namespace stappler::makefile

#endif /* CORE_MAKEFILE_SPMAKEFILEPROJECT_H_ */
