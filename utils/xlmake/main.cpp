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

using namespace sp;

static constexpr StringView XLMAKE_VERSION = "1.0";

namespace {

using namespace sp::makefile;
using namespace sp::mem_pool;

using PInterface = memory::PoolInterface;

// A GNU-make-style command-line variable assignment, e.g. CC=gcc, CFLAGS:=-O2, X+=y.
struct Assignment {
	StringView name;
	StringView op; // one of =, :=, ::=, :::=, +=, ?=
	StringView value;
};

struct Config {
	Vector<StringView> files; // -f / --file (repeatable)
	Vector<StringView> vars; // -V / --var (repeatable)
	Vector<StringView> targets; // positional goals
	Vector<Assignment> assignments; // positional VAR=VALUE command-line assignments
	StringView dir; // -C / --directory
	bool printVars = false; // -p / --print-vars
	bool recipe = false; // --recipe
	bool prereqs = false; // --prerequisites
	bool outOfDate = false; // -q / --out-of-date
	bool pedantic = false; // -W / --pedantic (enable every engine warning)
	bool help = false; // -h / --help
};

static void printUsage() {
	sprt::cout << "xlmake - inspect a GNU-make-style makefile\n"
				  "\n"
				  "Usage: xlmake [options] [target ...]\n"
				  "\n"
				  "Makefile selection (GNU make compatible):\n"
				  "  -f, --file FILE        read FILE as a makefile (repeatable)\n"
				  "  -C, --directory DIR    root directory (default: current directory;\n"
				  "                         searches GNUmakefile, makefile, Makefile)\n"
				  "\n"
				  "Variable assignment (GNU make compatible):\n"
				  "  VAR=VALUE              set VAR, overriding the makefile (also :=, +=, ?=)\n"
				  "\n"
				  "Inspection (combinable; act on the given targets, else the default goal):\n"
				  "  -p, --print-vars       print every variable: name [origin, flavor] = value\n"
				  "  -V, --var NAME         print one variable's expanded value (repeatable)\n"
				  "      --recipe           print the expanded recipe of each target\n"
				  "      --prerequisites    print the prerequisite list of each target\n"
				  "  -q, --out-of-date      restrict --recipe/--prerequisites to out-of-date items\n"
				  "  -W, --pedantic         report every engine warning (not just the default set)\n"
				  "  -h, --help             show this help\n"
				  "\n"
				  "With no action, prints an overview (makefile, default goal, targets).\n";
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
		while (opStart > 0 && arg[opStart - 1] == ':') {
			--opStart;
		}
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

// Parse argv into cfg. Returns false on a malformed option.
static bool parseArgs(int argc, const char *argv[], Config &cfg) {
	int i = 1;
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

	for (; i < argc; ++i) {
		StringView arg(argv[i]);
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
			if (name == "file" || name == "makefile") {
				cfg.files.emplace_back(hasVal ? val : takeValue(StringView()));
			} else if (name == "directory") {
				cfg.dir = hasVal ? val : takeValue(StringView());
			} else if (name == "var" || name == "variable") {
				cfg.vars.emplace_back(hasVal ? val : takeValue(StringView()));
			} else if (name == "print-vars" || name == "print") {
				cfg.printVars = true;
			} else if (name == "recipe" || name == "recipes") {
				cfg.recipe = true;
			} else if (name == "prerequisites" || name == "prereqs") {
				cfg.prereqs = true;
			} else if (name == "out-of-date") {
				cfg.outOfDate = true;
			} else if (name == "pedantic" || name == "warn-all") {
				cfg.pedantic = true;
			} else if (name == "help") {
				cfg.help = true;
			} else {
				sprt::cerr << "xlmake: unknown option --" << name << "\n";
				return false;
			}
		} else if (arg.size() > 1 && arg.is('-')) {
			auto rest = arg.sub(1);
			while (!rest.empty()) {
				char c = rest[0];
				rest = rest.sub(1);
				switch (c) {
				case 'f':
					cfg.files.emplace_back(takeValue(rest));
					rest = StringView();
					break;
				case 'C':
					cfg.dir = takeValue(rest);
					rest = StringView();
					break;
				case 'V':
					cfg.vars.emplace_back(takeValue(rest));
					rest = StringView();
					break;
				case 'p': cfg.printVars = true; break;
				case 'q': cfg.outOfDate = true; break;
				case 'W': cfg.pedantic = true; break;
				case 'h': cfg.help = true; break;
				default:
					sprt::cerr << "xlmake: unknown option -" << StringView(&c, 1) << "\n";
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

static void printVariable(Makefile *mk, StringView name, const Variable &v, ErrorReporter &err) {
	sprt::cout << name << " [" << getOriginName(v.origin) << ", ";
	switch (v.type) {
	case Variable::Type::String: sprt::cout << "simple] = " << v.str << "\n"; break;
	case Variable::Type::Function: sprt::cout << "function] = <function>\n"; break;
	case Variable::Type::Stmt:
		sprt::cout << "recursive] = ";
		mk->getVariableValue(name, [&](StringView s) { sprt::cout << s; }, err);
		sprt::cout << "\n";
		break;
	}
}

static int runXlmake(int argc, const char *argv[]) {
	Config cfg;
	if (!parseArgs(argc, argv, cfg)) {
		printUsage();
		return 1;
	}
	if (cfg.help) {
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

		bool anyAction = cfg.printVars || !cfg.vars.empty() || cfg.recipe || cfg.prereqs;

		// --- variables ---
		if (cfg.printVars) {
			// snapshot names first: expanding a recursive value may define new variables
			Vector<StringView> names;
			mk->foreachVariable(
					[&](StringView name, const Variable &) { names.emplace_back(name); });
			for (auto name : names) {
				if (auto v = mk->getVariable(name)) {
					printVariable(mk, name, *v, err);
				}
			}
		}

		for (auto name : cfg.vars) {
			auto v = mk->getVariable(name);
			if (!v) {
				sprt::cout << name << " = <undefined>\n";
				continue;
			}
			sprt::cout << name << " = ";
			mk->getVariableValue(name, [&](StringView s) { sprt::cout << s; }, err);
			sprt::cout << "\n";
		}

		// --- target-based actions ---
		if (cfg.recipe || cfg.prereqs) {
			// populate prerequisite / pattern-derived targets so they are queryable by name
			if (auto g = mk->getDefaultGoal()) {
				mk->buildPlan(g, err);
			}

			Vector<Target *> targets;
			if (cfg.targets.empty()) {
				if (auto g = mk->getDefaultGoal()) {
					targets.emplace_back(g);
				} else {
					sprt::cerr << "xlmake: no target given and no default goal\n";
				}
			} else {
				for (auto tn : cfg.targets) {
					if (auto t = mk->getTarget(tn)) {
						targets.emplace_back(t);
					} else {
						sprt::cerr << "xlmake: unknown target: " << tn << "\n";
					}
				}
			}

			if (cfg.recipe) {
				for (auto t : targets) {
					if (cfg.outOfDate && !mk->isOutOfDate(t, err)) {
						continue;
					}
					sprt::cout << t->name << ":\n";
					mk->exportRecipe(t,
							[&](StringView line) { sprt::cout << "\t" << line << "\n"; }, err);
				}
			}

			if (cfg.prereqs) {
				for (auto t : targets) {
					sprt::cout << t->name << ":";
					mk->getPrerequisites(t, [&](StringView name) {
						if (cfg.outOfDate) {
							auto pt = mk->getTarget(name);
							if (pt && !mk->isOutOfDate(pt, err)) {
								return;
							}
						}
						sprt::cout << " " << name;
					});
					sprt::cout << "\n";

					bool hasOrderOnly = false;
					mk->getOrderOnly(t, [&](StringView name) {
						if (!hasOrderOnly) {
							sprt::cout << "    | order-only:";
							hasOrderOnly = true;
						}
						sprt::cout << " " << name;
					});
					if (hasOrderOnly) {
						sprt::cout << "\n";
					}
				}
			}
		}

		// --- overview (no explicit action) ---
		if (!anyAction) {
			sprt::cout << "makefile:";
			for (auto &p : makefilePaths) { sprt::cout << " " << p; }
			sprt::cout << "\n";
			if (auto g = mk->getDefaultGoal()) {
				sprt::cout << "default goal: " << g->name << "\n";
			}
			sprt::cout << "targets:\n";
			for (auto t : mk->getTargets()) {
				sprt::cout << "  " << t->name;
				if (t->isPhony) {
					sprt::cout << " (phony)";
				}
				sprt::cout << "\n";
			}
		}
	}, pool);

	memory::pool::destroy(pool);
	return result;
}

} // namespace

int main(int argc, const char *argv[]) {
	return perform_main(argc, argv, [&]() { return runXlmake(argc, argv); });
}
