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

// Reusable project loading. Part of the makefile unity build (included from SPMakefile.cpp). The
// runner-agnostic GNU standard variables plus a one-call loader that yields a Stappler project's
// compile graph for read-only introspection. See SPMakefileProject.h.

#include "SPMakefileProject.h"
#include "SPFilesystem.h"

#include <stdlib.h> // getenv for lazy environment resolution

namespace STAPPLER_VERSIONIZED stappler::makefile {

void setupStandardVariables(Makefile *mk, StringView rootDir, ErrorReporter &err) {
	auto simple = [&](StringView n, StringView v) {
		mk->assignSimpleVariable(n, Origin::Default, v, err);
	};
	auto rec = [&](StringView n, StringView v) {
		mk->assignRecursiveVariable(n, Origin::Default, v, err);
	};

	// Program names
	simple("AR", "ar");
	simple("AS", "as");
	simple("CC", "cc");
	simple("CXX", "g++");
	simple("CO", "co");
	simple("FC", "f77");
	simple("GET", "get");
	simple("LD", "ld");
	simple("LEX", "lex");
	simple("LINT", "lint");
	simple("M2C", "m2c");
	simple("OBJC", "cc");
	simple("PC", "pc");
	simple("RM", "rm -f");
	simple("YACC", "yacc");
	simple("MAKEINFO", "makeinfo");
	simple("TEX", "tex");
	simple("TEXI2DVI", "texi2dvi");
	simple("TANGLE", "tangle");
	simple("WEAVE", "weave");
	simple("CTANGLE", "ctangle");
	simple("CWEAVE", "cweave");
	rec("CPP", "$(CC) -E");
	rec("F77", "$(FC)");
	rec("F77FLAGS", "$(FFLAGS)");

	// Default flags
	simple("ARFLAGS", "-rv");
	simple("COFLAGS", "");

	// Compile / link recipe templates (recursive: they track CC/CFLAGS/... when expanded)
	rec("COMPILE.c", "$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c");
	rec("COMPILE.cc", "$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c");
	rec("COMPILE.cpp", "$(COMPILE.cc)");
	rec("COMPILE.C", "$(COMPILE.cc)");
	rec("COMPILE.S", "$(CC) $(ASFLAGS) $(CPPFLAGS) $(TARGET_MACH) -c");
	rec("COMPILE.s", "$(AS) $(ASFLAGS) $(TARGET_MACH)");
	rec("LINK.c", "$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH)");
	rec("LINK.cc", "$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH)");
	rec("LINK.cpp", "$(LINK.cc)");
	rec("LINK.C", "$(LINK.cc)");
	rec("LINK.o", "$(CC) $(LDFLAGS) $(TARGET_ARCH)");
	rec("LINK.S", "$(CC) $(ASFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_MACH)");
	rec("LINK.s", "$(CC) $(ASFLAGS) $(LDFLAGS) $(TARGET_MACH)");
	rec("OUTPUT_OPTION", "-o $@");

	// Shell / recipe specials
	simple("SHELL", "/bin/sh");
	simple(".SHELLFLAGS", "-c");
	simple(".RECIPEPREFIX", "");

	// Suffix list and library search patterns
	simple("SUFFIXES",
			".out .a .ln .o .c .cc .C .cpp .p .f .F .m .r .y .l .ym .yl .s .S .mod .sym .def .h "
			".info .dvi .tex .texinfo .texi .txinfo .w .ch .web .sh .elc .el");
	rec(".LIBPATTERNS", "lib%.so lib%.a");

	// Recursive-make plumbing. MAKE defaults to a plain `make`; a driver that actually recurses
	// (e.g. xlmake) overrides MAKE_COMMAND with its own program path. MAKECMDGOALS is left empty
	// (== the default goal); a driver sets it from the requested goals.
	simple("MAKEFILES", "");
	simple("GNUMAKEFLAGS", "");
	simple("MAKEFLAGS", "");
	simple("MAKE_COMMAND", "make");
	rec("MAKE", "$(MAKE_COMMAND)");
	simple("MAKECMDGOALS", "");

	// CURDIR is make-visible and routinely joined into paths ($(CURDIR)/build/x.o), so a space in the
	// working directory must be encoded to PathSpacePlaceholder or the join would split into two words.
	//
	// The value has to OUTLIVE this call: a variable's Stmt keeps a StringView into the text it was
	// assigned from, it does not copy (every other assignment here passes a string literal). When the
	// path holds no space encodePathSpaces returns `rootDir` unchanged, which the caller owns — but
	// the encoded copy lives in a local buffer, so it must be duplicated into the makefile's pool.
	// Without this, CURDIR (and everything joined onto it) read freed memory the moment a project
	// path contained a space.
	mem_std::Interface::StringType curdirStorage;
	auto curdir = encodePathSpaces(rootDir, curdirStorage);
	if (curdir.data() != rootDir.data()) {
		curdir = curdir.pdup(mk->getPool());
	}
	simple("CURDIR", curdir);
}

Rc<MakefileRef> loadProject(StringView projectDir, SpanView<ProjectVariable> variables,
		ErrorReporter &err) {
	auto mk = Rc<MakefileRef>::create(SharedRefMode::Allocator);
	if (!mk) {
		err.reportError("loadProject: failed to create makefile");
		return nullptr;
	}

	mk->setRootPath(projectDir);

	// Lazy environment loading: resolve an otherwise-undefined variable from the process environment
	// on first use (Origin::Environment), without bulk-importing the whole environment.
	mk->addSubstitutionCallback(Origin::Environment,
			[](void *, const Callback<void(StringView)> &out, StringView name) -> bool {
		mem_std::Interface::StringType key(name.data(), name.size());
		if (const char *value = ::getenv(key.data())) {
			out(StringView(value));
			return true;
		}
		return false;
	}, nullptr);

	setupStandardVariables(mk, projectDir, err);

	// The Stappler build's universal.mk only expands the real source graph when STAPPLER_BUILD (or a
	// STAPPLER_TARGET) is set; without it, `all` is just a launcher that re-invokes $(MAKE). Set it
	// (Origin::CommandLine) so the full object/source graph is present. STAPPLER_TARGET is left to
	// default to the host — the current platform's target.
	mk->assignSimpleVariable("STAPPLER_BUILD", Origin::CommandLine, "1", err);

	// Caller-supplied command-line variables. They must land here — before the include — because the
	// build reads STAPPLER_TARGET/RELEASE at parse time; assigning them afterwards is a no-op.
	for (auto &var : variables) {
		mk->assignSimpleVariable(var.name, Origin::CommandLine, var.value, err);
	}

	// Find and include the project makefile by absolute path (so LOCAL_ROOT and $(shell find …) stay
	// absolute and no chdir is needed).
	mem_std::Interface::StringType path;
	for (auto def : {StringView("GNUmakefile"), StringView("makefile"), StringView("Makefile")}) {
		auto p = filepath::merge<mem_std::Interface>(projectDir, def);
		if (filesystem::exists(FileInfo{StringView(p)})) {
			path = sp::move(p);
			break;
		}
	}
	if (path.empty()) {
		err.reportError(toString("loadProject: no makefile found in ", projectDir,
				" (tried GNUmakefile, makefile, Makefile)"));
		return nullptr;
	}

	mk->includeFileByPath(StringView(path), &err);
	return mk;
}

} // namespace stappler::makefile
