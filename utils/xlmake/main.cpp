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

#include "SPCommon.h"
#include "SPFilesystem.h"
#include "SPFilepath.h"
#include "SPMakefile.h"

#include "Inspector.h"
#include "Executor.h"
#include <sys/utsname.h>
#include <dlfcn.h>
#include <stdlib.h> // getenv / setenv for MAKELEVEL recursion plumbing
#include <unistd.h> // chdir into the working directory (-C), so recursive $(MAKE) -C resolves right
#include <stdio.h> // setvbuf: line-buffer stdout so a sub-make's output streams to its parent live

using namespace sp;

static constexpr StringView XLMAKE_VERSION = "1.0";

namespace {

using namespace sp::makefile;
using namespace sp::mem_pool;

using PInterface = memory::PoolInterface;

// xlmake operates in one mode, chosen by the FIRST command-line flag; every later flag applies to
// that mode. -i/--inspect (pure introspection) or -b/--build (run recipes).
enum class Mode {
	Help,
	Inspect,
	Build,
};

// A GNU-make-style command-line variable assignment, e.g. CC=gcc, CFLAGS:=-O2, X+=y.
struct Assignment {
	StringView name;
	StringView op; // one of =, :=, ::=, :::=, +=, ?=
	StringView value;
};

struct Config {
	Mode mode = Mode::Help;

	// shared (both modes)
	Vector<StringView> files; // -f / --file (repeatable)
	StringView dir; // -C / --directory
	bool pedantic = false; // -W / --pedantic
	Vector<Assignment> assignments; // positional VAR=VALUE command-line assignments
	Vector<StringView> targets; // positional goals

	// inspect mode
	Vector<StringView> vars; // -V / --var (repeatable)
	bool printVars = false; // -p / --print-vars
	bool recipe = false; // --recipe
	bool prereqs = false; // --prerequisites
	bool recursive = false; // -r / --recursive
	bool outOfDate = false; // -q / --out-of-date
	bool phonyPrereqs = false; // -P / --phony-prereqs

	// build mode
	uint32_t jobs = 0; // -j N (0 => hardware_concurrency, JobsUnlimited => bare -j)
	bool keepGoing = false; // -k / --keep-going
	bool dryRun = false; // -n / --dry-run
	bool silent = false; // -s / --silent
	// build mode: GNU make-compatible data-extraction flags (used by the VSCode Makefile Tools ext)
	bool printDatabase = false; // -p / --print-data-base
	bool question = false; // -q / --question
	bool alwaysMake = false; // -B / --always-make
	bool printDirectory = false; // -w / --print-directory
	bool noPrintDirectory = false; // --no-print-directory (overrides -w and sub-make auto-enable)
};

static uint32_t s_makeLevel = 0;

static void printUsage() {
	sprt::cout << "xlmake - inspect or build a GNU-make-style makefile\n"
				  "\n"
				  "Usage: xlmake [options] [VAR=VALUE ...] [target ...]\n"
				  "\n"
				  "Build is the default mode (like make). A leading mode flag selects another:\n"
				  "  -b, --build            build the requested targets (default; run in parallel)\n"
				  "  -i, --inspect          inspect the makefile (print variables/recipes/prereqs)\n"
				  "  -h, --help             show this help\n"
				  "\n"
				  "Shared options (both modes):\n"
				  "  -f, --file FILE        read FILE as a makefile (repeatable)\n"
				  "  -C, --directory DIR    root directory (default: current directory;\n"
				  "                         searches GNUmakefile, makefile, Makefile)\n"
				  "  -W, --pedantic         report every engine warning (not just the default set)\n"
				  "  VAR=VALUE              set VAR, overriding the makefile (also :=, +=, ?=)\n"
				  "\n"
				  "Inspect options (-i; combinable; act on the given targets, else the default "
				  "goal):\n"
				  "  -p, --print-vars       print every variable: name [origin, flavor] = value\n"
				  "  -V, --var NAME         print one variable's expanded value (repeatable)\n"
				  "      --recipe           print the expanded recipe of each target\n"
				  "      --prerequisites    print the prerequisite list of each target\n"
				  "  -r, --recursive        with --prerequisites, print the transitive closure in\n"
				  "                         dependency-graph order; with -q, the out-of-date set\n"
				  "  -q, --out-of-date      restrict --recipe/--prerequisites to out-of-date items\n"
				  "  -P, --phony-prereqs    with -r -q, judge a phony target by its prerequisites\n"
				  "\n"
				  "Build options (default mode):\n"
				  "  -j, --jobs [N]         run up to N recipes concurrently (default: all cores;\n"
				  "                         -j1 serializes; bare -j is unlimited)\n"
				  "  -k, --keep-going       keep building independent targets after a failure\n"
				  "  -n, --dry-run          print recipe commands without running them\n"
				  "  -s, --silent           do not echo recipe command lines\n"
				  "  -B, --always-make      consider every target out of date (force a rebuild)\n"
				  "\n"
				  "make-compatibility options (default mode; for tooling such as VSCode):\n"
				  "  -p, --print-data-base  dump the makefile database (variables, targets) and exit\n"
				  "  -q, --question         run no recipe; exit 1 if any target is out of date\n"
				  "  -w, --print-directory  print 'Entering/Leaving directory' around the build\n"
				  "      --no-builtin-rules, --no-builtin-variables   accepted, no effect (-r, -R)\n"
				  "\n"
				  "With -i and no action, prints an overview (makefile, default goal, targets).\n";
}

static void xlmakeLog(void *, log::LogType type, StringView msg) {
	String label = s_makeLevel > 0 ? toString("xlmake[", s_makeLevel, "]") : toString("xlmake");
	StringView tag = (type == log::LogType::Error) ? StringView("error")
			: (type == log::LogType::Warn)		   ? StringView("warning")
												   : StringView("info");
	sprt::cerr << label << ": " << tag << ": " << msg << "\n";
}

// Recognize a GNU-make-style command-line assignment "NAME<op>VALUE" (op: =, :=, ::=,
// :::=, +=, ?=). Returns false (so the arg is treated as a target) when there is no '=',
// the name is empty, or the name contains whitespace.
static bool parseAssignment(StringView arg, Assignment &out) {
	auto eq = arg.find('=');
	if (eq == maxOf<size_t>() || eq == 0) {
		return false;
	}

	size_t opStart = eq;
	char prev = arg[eq - 1];
	if (prev == ':') {
		while (opStart > 0 && arg[opStart - 1] == ':') { --opStart; }
	} else if (prev == '?' || prev == '+') {
		opStart = eq - 1;
	}

	auto name = arg.sub(0, opStart);
	name.trimChars<StringView::WhiteSpace>();
	if (name.empty() || name.find(' ') != maxOf<size_t>() || name.find('\t') != maxOf<size_t>()) {
		return false;
	}

	auto value = arg.sub(eq + 1);
	value.trimChars<StringView::WhiteSpace>();

	out.name = name;
	out.op = arg.sub(opStart, (eq + 1) - opStart);
	out.value = value;
	return true;
}

// Parse a non-negative decimal integer; false if `s` is empty or has a non-digit.
static bool parseUint(StringView s, uint32_t &out) {
	if (s.empty()) {
		return false;
	}
	uint32_t v = 0;
	for (size_t k = 0; k < s.size(); ++k) {
		char c = s[k];
		if (c < '0' || c > '9') {
			return false;
		}
		v = v * 10 + uint32_t(c - '0');
	}
	out = v;
	return true;
}

// Parse argv into cfg. The first token selects the mode. Returns false on a malformed option.
static bool parseArgs(int argc, const char *argv[], Config &cfg) {
	// Build is the default mode (like make); an explicit -i/-b (or -h) as the FIRST argument
	// selects another mode. Otherwise argv[1] is a normal option/assignment/target.
	int i = 1;
	if (argc >= 2) {
		StringView first(argv[1]);
		if (first == "-h" || first == "--help") {
			cfg.mode = Mode::Help;
			return true;
		} else if (first == "-i" || first == "--inspect") {
			cfg.mode = Mode::Inspect;
			i = 2;
		} else if (first == "-b" || first == "--build") {
			cfg.mode = Mode::Build;
			i = 2;
		} else {
			cfg.mode = Mode::Build; // default: behave like make
		}
	} else {
		cfg.mode = Mode::Build; // bare `xlmake`: build the default goal
		return true;
	}

	const bool isBuild = (cfg.mode == Mode::Build);
	const bool isInspect = (cfg.mode == Mode::Inspect);

	auto takeValue = [&](StringView inlineVal) -> StringView {
		if (!inlineVal.empty()) {
			return inlineVal;
		}
		if (i + 1 < argc) {
			++i;
			return StringView(argv[i]);
		}
		return StringView();
	};
	// -j with an optional argument: an attached/next numeric value sets the cap, otherwise bare -j
	// means unlimited.
	auto takeJobs = [&](StringView attached) -> bool {
		if (!attached.empty()) {
			uint32_t n;
			if (!parseUint(attached, n)) {
				sprt::cerr << "xlmake: invalid -j value: " << attached << "\n";
				return false;
			}
			cfg.jobs = n ? n : 1;
			return true;
		}
		if (i + 1 < argc) {
			StringView nx(argv[i + 1]);
			uint32_t n;
			if (parseUint(nx, n)) {
				++i;
				cfg.jobs = n ? n : 1;
				return true;
			}
		}
		cfg.jobs = xlmake::JobsUnlimited;
		return true;
	};

	for (; i < argc; ++i) {
		StringView arg(argv[i]);
		if (arg == "-h" || arg == "--help") {
			cfg.mode = Mode::Help;
			return true;
		}
		if (arg.starts_with("--")) {
			auto body = arg.sub(2);
			if (body.empty()) {
				continue; // bare "--": ignore
			}
			StringView name = body, val;
			bool hasVal = false;
			auto eq = body.find('=');
			if (eq != maxOf<size_t>()) {
				name = body.sub(0, eq);
				val = body.sub(eq + 1);
				hasVal = true;
			}

			// shared
			if (name == "file" || name == "makefile") {
				cfg.files.emplace_back(hasVal ? val : takeValue(StringView()));
			} else if (name == "directory") {
				cfg.dir = hasVal ? val : takeValue(StringView());
			} else if (name == "pedantic" || name == "warn-all") {
				cfg.pedantic = true;
			} else if (isInspect && (name == "var" || name == "variable")) {
				cfg.vars.emplace_back(hasVal ? val : takeValue(StringView()));
			} else if (isInspect && (name == "print-vars" || name == "print")) {
				cfg.printVars = true;
			} else if (isInspect && (name == "recipe" || name == "recipes")) {
				cfg.recipe = true;
			} else if (isInspect && (name == "prerequisites" || name == "prereqs")) {
				cfg.prereqs = true;
			} else if (isInspect && name == "recursive") {
				cfg.recursive = true;
			} else if (isInspect && name == "out-of-date") {
				cfg.outOfDate = true;
			} else if (isInspect && (name == "phony-prereqs" || name == "phony-prerequisites")) {
				cfg.phonyPrereqs = true;
			} else if (isBuild && name == "jobs") {
				if (!takeJobs(hasVal ? val : StringView())) {
					return false;
				}
			} else if (isBuild && name == "keep-going") {
				cfg.keepGoing = true;
			} else if (isBuild && (name == "dry-run" || name == "just-print" || name == "recon")) {
				cfg.dryRun = true;
			} else if (isBuild && (name == "silent" || name == "quiet")) {
				cfg.silent = true;
			} else if (isBuild && (name == "print-data-base" || name == "print-database")) {
				cfg.printDatabase = true;
			} else if (isBuild && name == "question") {
				cfg.question = true;
			} else if (isBuild && name == "always-make") {
				cfg.alwaysMake = true;
			} else if (isBuild && name == "print-directory") {
				cfg.printDirectory = true;
			} else if (isBuild && name == "no-print-directory") {
				cfg.noPrintDirectory = true;
			} else if (isBuild && (name == "no-builtin-rules" || name == "no-builtin-variables")) {
				// accepted no-op: xlmake has no builtin rule/variable database
			} else {
				sprt::cerr << "xlmake: unknown option --" << name << "\n";
				return false;
			}
		} else if (arg.size() > 1 && arg.is('-')) {
			auto rest = arg.sub(1);
			while (!rest.empty()) {
				char c = rest[0];
				rest = rest.sub(1);
				bool ok = true;
				// shared short options first
				if (c == 'f') {
					cfg.files.emplace_back(takeValue(rest));
					rest = StringView();
				} else if (c == 'C') {
					cfg.dir = takeValue(rest);
					rest = StringView();
				} else if (c == 'W') {
					cfg.pedantic = true;
				} else if (isInspect) {
					switch (c) {
					case 'V':
						cfg.vars.emplace_back(takeValue(rest));
						rest = StringView();
						break;
					case 'p': cfg.printVars = true; break;
					case 'q': cfg.outOfDate = true; break;
					case 'r': cfg.recursive = true; break;
					case 'P': cfg.phonyPrereqs = true; break;
					default: ok = false; break;
					}
				} else { // build
					switch (c) {
					case 'j':
						if (!takeJobs(rest)) {
							return false;
						}
						rest = StringView();
						break;
					case 'k': cfg.keepGoing = true; break;
					case 'n': cfg.dryRun = true; break;
					case 's': cfg.silent = true; break;
					case 'p': cfg.printDatabase = true; break;
					case 'q': cfg.question = true; break;
					case 'B': cfg.alwaysMake = true; break;
					case 'w': cfg.printDirectory = true; break;
					case 'r': break; // --no-builtin-rules: accepted no-op
					case 'R': break; // --no-builtin-variables: accepted no-op
					default: ok = false; break;
					}
				}
				if (!ok) {
					sprt::cerr << "xlmake: unknown option -" << StringView(&c, 1)
							   << (isBuild ? " for -b/build mode\n" : " for -i/inspect mode\n");
					return false;
				}
			}
		} else {
			Assignment a;
			if (parseAssignment(arg, a)) {
				cfg.assignments.emplace_back(a);
			} else {
				cfg.targets.emplace_back(arg);
			}
		}
	}
	return true;
}

// Define GNU make's standard predefined variables (origin "default", so makefile and command-line
// assignments still override them, matching GNU). Covers the toolchain program names, the
// compile/link recipe templates, the recipe/shell specials, and the per-invocation variables
// (MAKE/MAKE_COMMAND for recursion, CURDIR, MAKECMDGOALS). `makeCommand` is argv[0] so `$(MAKE)`
// re-invokes this xlmake; `rootDir` is the absolute working directory (after -C).
static void setupStandardVariables(Makefile *mk, StringView rootDir, StringView makeCommand,
		const Vector<StringView> &goals, ErrorReporter &err) {
	using O = makefile::Origin;
	auto simple = [&](StringView n, StringView v) {
		mk->assignSimpleVariable(n, O::Default, v, err); //
	};

	auto rec = [&](StringView n, StringView v) {
		mk->assignRecursiveVariable(n, O::Default, v, err); //
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

	// Shell / recipe specials (xlmake runs recipes via /bin/sh -c)
	simple("SHELL", "/bin/sh");
	simple(".SHELLFLAGS", "-c");
	simple(".RECIPEPREFIX", "");

	// Suffix list and library search patterns
	simple("SUFFIXES",
			".out .a .ln .o .c .cc .C .cpp .p .f .F .m .r .y .l .ym .yl .s .S .mod .sym .def .h "
			".info .dvi .tex .texinfo .texi .txinfo .w .ch .web .sh .elc .el");
	rec(".LIBPATTERNS", "lib%.so lib%.a");

	// Recursive-make plumbing: $(MAKE) re-invokes this xlmake (argv[0]).
	simple("MAKEFILES", "");
	simple("GNUMAKEFLAGS", "");
	simple("MAKEFLAGS", "");
	simple("MAKE_COMMAND", makeCommand);
	rec("MAKE", "$(MAKE_COMMAND)");

	// Per-invocation
	simple("CURDIR", rootDir);
	String goalList;
	for (auto &g : goals) {
		if (!goalList.empty()) {
			goalList.append(" ");
		}
		goalList.append(g.data(), g.size());
	}
	simple("MAKECMDGOALS", goalList);
}

static String resolvePath(StringView root, StringView file) {
	if (filepath::isAbsolute(file)) {
		return filepath::reconstructPath<PInterface>(file);
	}
	return filepath::reconstructPath<PInterface>(filepath::merge<PInterface>(root, file));
}

// Format a small unsigned into `buf` (needs >= 12 bytes) and return a null-terminated view into it
// — usable both as a StringView (size) and as a C string (setenv needs the trailing NUL).
static StringView formatUint(uint32_t v, char *buf, size_t cap) {
	size_t i = cap;
	buf[--i] = '\0';
	if (v == 0) {
		buf[--i] = '0';
	} else {
		while (v && i) {
			buf[--i] = char('0' + (v % 10));
			v /= 10;
		}
	}
	return StringView(buf + i);
}

// Current recursion depth, read from the environment like GNU make's MAKELEVEL (absent => 0; a
// non-numeric value is treated as 0, matching make's tolerant parse).
static uint32_t readMakeLevel() {
	const char *env = ::getenv("MAKELEVEL");
	if (!env) {
		return 0;
	}
	uint32_t v = 0;
	for (const char *p = env; *p >= '0' && *p <= '9'; ++p) { v = v * 10 + uint32_t(*p - '0'); }
	return v;
}

static int runXlmake(int argc, const char *argv[]) {
	// Line-buffer stdout. A sub-make's stdout is a pipe to its parent, where libc would otherwise
	// fully buffer it and only flush at exit — defeating the parent's live, per-line streaming of
	// recursive output. Line buffering flushes each completed line immediately (a no-op change for a
	// terminal, which is already line-buffered).
	::setvbuf(stdout, nullptr, _IOLBF, BUFSIZ);

	Config cfg;
	if (!parseArgs(argc, argv, cfg)) {
		printUsage();
		return 1;
	}
	if (cfg.mode == Mode::Help) {
		printUsage();
		return 0;
	}

	int result = 0;
	auto pool = memory::pool::create((memory::pool_t *)nullptr);

	memory::perform([&] {
		auto rootDir = filesystem::currentDir<PInterface>(cfg.dir);
		if (rootDir.empty()) {
			sprt::cerr << "xlmake: cannot resolve directory: "
					   << (cfg.dir.empty() ? StringView(".") : cfg.dir) << "\n";
			result = 1;
			return;
		}
		StringView rootView(rootDir);

		// Match GNU make's -C: actually change into the working directory, so recipes (and any
		// nested `$(MAKE) -C ...` they invoke) run there and resolve relative paths correctly.
		// Without -C, rootDir is the launch directory and this is a no-op.
		if (::chdir(rootDir.data()) != 0) {
			sprt::cerr << "xlmake: cannot change directory to: " << rootView << "\n";
			result = 1;
			return;
		}

		// resolve the makefile(s) to read
		Vector<String> makefilePaths;
		if (cfg.files.empty()) {
			for (auto def :
					{StringView("GNUmakefile"), StringView("makefile"), StringView("Makefile")}) {
				auto p = resolvePath(rootView, def);
				if (filesystem::exists(FileInfo{StringView(p)})) {
					makefilePaths.emplace_back(p);
					break;
				}
			}
			if (makefilePaths.empty()) {
				sprt::cerr << "xlmake: no makefile found in " << rootView
						   << " (tried GNUmakefile, makefile, Makefile)\n";
				result = 1;
				return;
			}
		} else {
			for (auto f : cfg.files) {
				auto p = resolvePath(rootView, f);
				if (!filesystem::exists(FileInfo{StringView(p)})) {
					sprt::cerr << "xlmake: makefile not found: " << p << "\n";
					result = 1;
					return;
				}
				makefilePaths.emplace_back(p);
			}
		}

		auto mk = Rc<MakefileRef>::create(SharedRefMode::Allocator);

		mk->assignSimpleVariable("XLMAKE_VERSION", Origin::Default, XLMAKE_VERSION);

		// Recursive-make depth (GNU MAKELEVEL). Read our level from the environment, expose it to
		// the makefile (origin "environment", so a makefile assignment can still override it), and
		// export level+1 to every recipe child via the process environment — so any `$(MAKE)` the
		// recipes invoke starts one level deeper, exactly like GNU make. (Children are exec'd with
		// our environ, so setenv() is the propagation channel.)
		uint32_t makeLevel = readMakeLevel();

		s_makeLevel = makeLevel;

		char curLevelBuf[12];
		char nextLevelBuf[12];
		mk->assignSimpleVariable("MAKELEVEL", Origin::Environment,
				formatUint(makeLevel, curLevelBuf, sizeof(curLevelBuf)));
		::setenv("MAKELEVEL", formatUint(makeLevel + 1, nextLevelBuf, sizeof(nextLevelBuf)).data(),
				1);

		// Diagnostic colour. clang/gcc emit ANSI colour only when their stdout/stderr is a TTY, but
		// every recipe's output is captured through a pipe (so a target's block stays contiguous) —
		// the compiler therefore sees a pipe and goes monochrome. Decide once whether the build should
		// force colour and let the makefiles act on it: $(XLMAKE_COLOR) gates -fdiagnostics-color=always
		// (understood by both gcc and clang). At the top level the choice follows xlmake's OWN stdout;
		// a sub-make inherits the parent's decision from the environment — its stdout is the pipe back
		// to the parent, where isatty() would wrongly answer "no". Expose it to this makefile (origin
		// "environment", so a makefile or command line may still override) and re-export it so any
		// recursive $(MAKE) inherits the same choice. CLICOLOR_FORCE is exported alongside as a courtesy
		// to the many CLICOLOR-aware tools (ls, grep, cmake, ...) a recipe may invoke; the compiler
		// ignores it, which is exactly why $(XLMAKE_COLOR) drives the compiler flag instead.
		bool color;
		if (const char *colorEnv = ::getenv("XLMAKE_COLOR")) {
			color = (colorEnv[0] == '1');
		} else {
			color = ::isatty(STDOUT_FILENO) != 0
					|| sprt::hasFlag(sprt::oslog::LogFeatures::acquire().features,
							sprt::oslog::LogFeatures::AnsiCompatible);
		}
		mk->assignSimpleVariable("XLMAKE_COLOR", Origin::Environment,
				color ? StringView("1") : StringView("0"));
		::setenv("XLMAKE_COLOR", color ? "1" : "0", 1);
		if (color) {
			::setenv("CLICOLOR_FORCE", "1", 1);
		}

		if (auto h = ::getenv("HOME")) {
			mk->assignSimpleVariable("HOME", Origin::Environment, StringView(h));
		}

		utsname unamebuf;
		uname(&unamebuf);

		mk->assignSimpleVariable("XL_UNAME_SYSNAME", Origin::Default, unamebuf.sysname);
		mk->assignSimpleVariable("XL_UNAME_NODENAME", Origin::Default, unamebuf.nodename);
		mk->assignSimpleVariable("XL_UNAME_RELEASE", Origin::Default, unamebuf.release);
		mk->assignSimpleVariable("XL_UNAME_VERSION", Origin::Default, unamebuf.version);
		mk->assignSimpleVariable("XL_UNAME_MACHINE", Origin::Default, unamebuf.machine);
		mk->assignSimpleVariable("XL_UNAME_DOMAINNAME", Origin::Default, unamebuf.domainname);

		auto gnu_get_libc_version =
				(const char *(*)())::dlsym(RTLD_DEFAULT, "gnu_get_libc_version");
		if (gnu_get_libc_version != nullptr) {
			mk->assignSimpleVariable("XL_GLIBC_VERSION", Origin::Default, gnu_get_libc_version());
		}

		mk->setLogCallback(xlmakeLog);
		if (cfg.pedantic) {
			mk->setFlags(EngineFlags::WarnAll);
		}
		mk->setRootPath(rootView);

		// resolve nested `include` directives against the root directory
		mk->setIncludeCallback([](void *ud, StringView path, const Makefile::PathCallback &cb) {
			auto &root = *reinterpret_cast<StringView *>(ud);
			auto abs = resolvePath(root, path);
			auto data = filesystem::readTextFile<PInterface>(FileInfo{StringView(abs)});
			if (!data.empty()) {
				cb(data);
			}
		}, &rootView);

		ErrorReporter err(nullptr);
		err.callback = xlmakeLog;
		err.filename = StringView("xlmake");

		// GNU make's standard predefined variables (origin "default"): toolchain names, recipe
		// templates, $(MAKE)/$(CURDIR)/$(MAKECMDGOALS), etc. Set before the command-line
		// assignments and the makefile read so both can override them, matching GNU.
		setupStandardVariables(mk, rootView, StringView(argv[0]), cfg.targets, err);

		// Apply command-line variable assignments before reading the makefile, with
		// Origin::CommandLine so a plain makefile assignment cannot override them (an
		// `override` directive still can), matching GNU make.
		for (auto &a : cfg.assignments) {
			if (a.op == ":=" || a.op == "::=" || a.op == ":::=") {
				mk->assignSimpleVariable(a.name, Origin::CommandLine, a.value, err);
			} else if (a.op == "+=") {
				mk->appendToVariable(a.name, Origin::CommandLine, a.value, err);
			} else if (a.op == "?=") {
				if (!mk->getVariable(a.name)) {
					mk->assignRecursiveVariable(a.name, Origin::CommandLine, a.value, err);
				}
			} else { // "="
				mk->assignRecursiveVariable(a.name, Origin::CommandLine, a.value, err);
			}
		}

		for (auto &p : makefilePaths) {
			// non-fatal: report and keep going so partial inspection still works
			mk->include(FileInfo{StringView(p)}, &err);
		}

		if (cfg.mode == Mode::Inspect) {
			xlmake::InspectConfig ic;
			ic.vars = cfg.vars;
			ic.targets = cfg.targets;
			ic.printVars = cfg.printVars;
			ic.recipe = cfg.recipe;
			ic.prereqs = cfg.prereqs;
			ic.recursive = cfg.recursive;
			ic.outOfDate = cfg.outOfDate;
			ic.phonyPrereqs = cfg.phonyPrereqs;
			result = xlmake::runInspect(mk, ic, makefilePaths, err);
		} else {
			xlmake::BuildConfig bc;
			bc.targets = cfg.targets;
			bc.jobs = cfg.jobs;
			bc.keepGoing = cfg.keepGoing;
			bc.dryRun = cfg.dryRun;
			bc.silent = cfg.silent;
			bc.printDatabase = cfg.printDatabase;
			bc.question = cfg.question;
			bc.alwaysMake = cfg.alwaysMake;
			bc.makeLevel = makeLevel;
			// GNU make prints Entering/Leaving directory for a sub-make (MAKELEVEL > 0) or when -w
			// is given, unless --no-print-directory was passed.
			bc.printDirectory = !cfg.noPrintDirectory && (cfg.printDirectory || makeLevel > 0);
			bc.rootDir = rootView;
			result = xlmake::runBuild(mk, bc, err);
		}
	}, pool);

	memory::pool::destroy(pool);
	return result;
}

} // namespace

int main(int argc, const char *argv[]) {
	return perform_main(argc, argv, [&]() { return runXlmake(argc, argv); });
}
