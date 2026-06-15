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
};

static void printUsage() {
	sprt::cout << "xlmake - inspect or build a GNU-make-style makefile\n"
				  "\n"
				  "Usage: xlmake <mode> [options] [VAR=VALUE ...] [target ...]\n"
				  "\n"
				  "The first argument selects the mode; later flags apply to it:\n"
				  "  -i, --inspect          inspect the makefile (print variables/recipes/prereqs)\n"
				  "  -b, --build            build the requested targets (run recipes in parallel)\n"
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
				  "Build options (-b):\n"
				  "  -j, --jobs [N]         run up to N recipes concurrently (default: all cores;\n"
				  "                         -j1 serializes; bare -j is unlimited)\n"
				  "  -k, --keep-going       keep building independent targets after a failure\n"
				  "  -n, --dry-run          print recipe commands without running them\n"
				  "  -s, --silent           do not echo recipe command lines\n"
				  "\n"
				  "With -i and no action, prints an overview (makefile, default goal, targets).\n";
}

static void xlmakeLog(void *, log::LogType type, StringView msg) {
	StringView tag = (type == log::LogType::Error) ? StringView("error")
			: (type == log::LogType::Warn)		   ? StringView("warning")
												   : StringView("info");
	sprt::cerr << "xlmake: " << tag << ": " << msg << "\n";
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
	if (argc < 2) {
		cfg.mode = Mode::Help;
		return true;
	}

	StringView first(argv[1]);
	if (first == "-h" || first == "--help") {
		cfg.mode = Mode::Help;
		return true;
	} else if (first == "-i" || first == "--inspect") {
		cfg.mode = Mode::Inspect;
	} else if (first == "-b" || first == "--build") {
		cfg.mode = Mode::Build;
	} else {
		sprt::cerr << "xlmake: first argument must select a mode: -i/--inspect, -b/--build "
					  "(or -h/--help)\n";
		return false;
	}

	const bool isBuild = (cfg.mode == Mode::Build);
	const bool isInspect = (cfg.mode == Mode::Inspect);

	int i = 2;
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
			} else if (isBuild && (name == "dry-run" || name == "just-print")) {
				cfg.dryRun = true;
			} else if (isBuild && (name == "silent" || name == "quiet")) {
				cfg.silent = true;
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

static String resolvePath(StringView root, StringView file) {
	if (filepath::isAbsolute(file)) {
		return filepath::reconstructPath<PInterface>(file);
	}
	return filepath::reconstructPath<PInterface>(filepath::merge<PInterface>(root, file));
}

static int runXlmake(int argc, const char *argv[]) {
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

		mk->assignSimpleVariable("XLMAKE_VERSION", makefile::Origin::Default, XLMAKE_VERSION);

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
