/**
Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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
#include "SPString.h"
#include "SPMemInterface.h"

#include "SPFilesystem.h"
#include "SPFilepath.h"
#include "SPMakefile.h"
//#include "SPThread.h"

//#include "SPData.h"
//#include "SPDataValue.h"

#include <sprt/runtime/platform.h>
#include <sprt/runtime/utils/backtrace.h>
#include <sprt/runtime/utils/compress.h>
#include <sprt/runtime/utils/idn.h>

using namespace stappler;

/*static sprt::rmutex s_mutex;

class TestThread : public thread::Thread {
public:
	virtual void threadInit() override {
		sprt::unique_lock lock(s_mutex);
		Thread::threadInit();
		slog().debug("Thread", "threadInit: ", getThreadId());
	}
	virtual void threadDispose() override {
		sprt::unique_lock lock(s_mutex);
		Thread::threadDispose();
		slog().debug("Thread", "threadDispose: ", getThreadId());
	}
	virtual bool worker() override {
		sprt::unique_lock lock(s_mutex);
		slog().debug("Thread", "worker: ", getThreadId());
		return false;
	}
};

static void performIdnTests() {
	sprt::idn::puny_encode([](StringView str) {
		std::cout << str << "\n"; //
	}, "рф", true);

	sprt::idn::puny_decode([](StringView str) {
		std::cout << str << "\n"; //
	}, "p1ai", true);

	sprt::idn::puny_decode([](StringView str) {
		std::cout << str << "\n"; //
	}, "xn--p1ai", true);

	sprt::idn::puny_decode([](StringView str) {
		std::cout << str << "\n"; //
	}, "XN--P1AI", true);

	std::cout << sprt::idn::is_known_tld("рф") << "\n";
}

static void performThreadTests() {
	s_mutex.lock();

	auto t = Rc<TestThread>::create();
	t->run();

	sprt::platform::sleep(1'000);

	slog().debug("Thread", "performThreadTests");

	s_mutex.unlock();

	t->waitStopped();
}

static void performDynAllocTests() {
	sprt::String str;
	str += "test 1234567890 1234567890 1234567890\n";

	std::cout << str;

	auto str2 = sprt::StreamTraits<char>::toString("test", 1, " ", 0.56, " 123456 |", '\n');

	std::cout << str2;
}

static void performPathTests() {
	std::cout << "UniqueDeviceId: " << sprt::platform::getUniqueDeviceId() << "\n";
	std::cout << "ExecPath: " << sprt::platform::getExecPath() << "\n";
	std::cout << "HomePath: " << sprt::platform::getHomePath() << "\n";

	for (auto it : each<LocationCategory>()) {
		filesystem::enumeratePaths(it, [&](const LocationInfo &, StringView path) {
			std::cout << it << ": " << path << "\n";
			return true;
		});
	}

	auto execDir = filepath::root(sprt::platform::getExecPath());

	filesystem::copy(FileInfo("exec_objs", LocationCategory::Bundled),
			FileInfo("", LocationCategory::AppRuntime));

	filesystem::move(FileInfo("exec_objs", LocationCategory::AppRuntime),
			FileInfo("exec_objs", LocationCategory::AppCache));

	filesystem::remove(FileInfo("exec_objs", LocationCategory::AppCache), true);

	filesystem::ftw(FileInfo(execDir), [](const FileInfo &info, FileType t) {
		std::cout << info << " (" << t << ")\n";
		return true;
	});

	filesystem::ftw(FileInfo("exec_objs", LocationCategory::Bundled),
			[](const FileInfo &info, FileType t) {
		std::cout << info << " (" << t << ")\n";
		return true;
	});
}

static void performTimeTests() {
	char timebuf[sprt::time::time_exp_t::Iso8601BufferSize] = {0};
	auto tm1 = sprt::time::time_exp_t::get(false);
	tm1.encodeIso8601(timebuf, sprt::time::time_exp_t::Iso8601BufferSize, 6);
	std::cout << timebuf << "\n";
	tm1.encodeRfc822(timebuf, sprt::time::time_exp_t::Iso8601BufferSize);
	std::cout << timebuf << "\n";

	auto tm2 = sprt::time::time_exp_t::get(true);
	tm2.encodeIso8601(timebuf, sprt::time::time_exp_t::Iso8601BufferSize, 6);
	std::cout << timebuf << "\n";
	tm2.encodeRfc822(timebuf, sprt::time::time_exp_t::Iso8601BufferSize);
	std::cout << timebuf << "\n";

	auto tnow = sprt::platform::clock(sprt::platform::ClockType::Realtime);
	sprt::time::time_exp_t tm3(tnow);
	tm3.encodeIso8601(timebuf, sprt::time::time_exp_t::Iso8601BufferSize, 6);
	std::cout << timebuf << "\n";
	tm3.encodeRfc822(timebuf, sprt::time::time_exp_t::Iso8601BufferSize);
	std::cout << timebuf << "\n";

	sprt::time::time_exp_t tm4(tnow, true);
	tm4.encodeIso8601(timebuf, sprt::time::time_exp_t::Iso8601BufferSize, 6);
	std::cout << timebuf << "\n";
	tm4.encodeRfc822(timebuf, sprt::time::time_exp_t::Iso8601BufferSize);
	std::cout << timebuf << "\n";

	sprt::time::time_exp_t tm5("2025-12-31T20:21:28.039509+08:00");
	tm5.encodeIso8601(timebuf, sprt::time::time_exp_t::Iso8601BufferSize, 6);
	std::cout << timebuf << "\n";
}

static void performUnicodeTests() {
	StringView test1 = "Тест";
	StringView test2 = "ТЕСТ";
	StringView test3 = "ТЕСТ";

	WideStringView wtest1 = u"Тест1";
	WideStringView wtest2 = u"ТЕСТ1";
	WideStringView wtest3 = u"тест1";

	std::cout << platform::toupper<memory::StandartInterface>(test1) << "\n";
	std::cout << platform::tolower<memory::StandartInterface>(test1) << "\n";
	std::cout << platform::totitle<memory::StandartInterface>(test1) << "\n";

	std::cout << platform::toupper<memory::StandartInterface>(test2) << "\n";
	std::cout << platform::tolower<memory::StandartInterface>(test2) << "\n";
	std::cout << platform::totitle<memory::StandartInterface>(test2) << "\n";

	std::cout << "StringUnicodeCaseComparator: "
			  << test1.equals<sprt::StringUnicodeCaseComparator>(test2) << "\n";
	std::cout << "StringCaseComparator: " << test1.equals<sprt::StringCaseComparator>(test2)
			  << "\n";

	std::cout << "StringUnicodeCaseComparator: "
			  << wtest1.equals<sprt::StringUnicodeCaseComparator>(wtest2) << "\n";
	std::cout << "StringCaseComparator: " << wtest1.equals<sprt::StringCaseComparator>(wtest2)
			  << "\n";

	std::cout << (test3 < test1) << " " << (test3 > test1) << '\n';
}*/

// Exercises the makefile module's executor / introspection / recipe-export API end to end.
// Returns the number of failed checks (0 == success).
static int performMakefileTests() {
	using namespace stappler::makefile;
	using namespace stappler::mem_pool;

	int failures = 0;
	auto pool = memory::pool::create((memory::pool_t *)nullptr);

	memory::perform([&] {
		auto logcb = [](void *, log::LogType, StringView msg) {
			sprt::cout << "    [makefile] " << msg << "\n";
		};

		auto check = [&](bool cond, StringView name) {
			sprt::cout << (cond ? "[ OK ] " : "[FAIL] ") << name << "\n";
			if (!cond) {
				++failures;
			}
		};
		auto checkEq = [&](StringView got, StringView expect, StringView name) {
			bool ok = (got == expect);
			sprt::cout << (ok ? "[ OK ] " : "[FAIL] ") << name;
			if (!ok) {
				sprt::cout << "  (got \"" << got << "\", expected \"" << expect << "\")";
			}
			sprt::cout << "\n";
			if (!ok) {
				++failures;
			}
		};

		// --- fixture A: variables, functions, $(eval), pattern rules, automatic vars ---
		static constexpr StringView kFixture =
				"CC := cc\n"
				"CFLAGS := -O2\n"
				"OBJS := foo.o bar.o\n"
				"\n"
				"all: app\n"
				"\n"
				"app: $(OBJS)\n"
				"\t$(CC) $(CFLAGS) -o $@ $^\n"
				"\n"
				"%.o: %.c\n"
				"\t$(CC) $(CFLAGS) -c -o $@ $<\n"
				"\n"
				"obj/%.o: src/%.c\n"
				"\t@echo build $* : $@ from $< [$(@D)/$(<F)]\n"
				"\n"
				".PHONY: all clean\n"
				"clean:\n"
				"\trm -f $(OBJS) app\n"
				"\n"
				"extra: obj/foo.o\n"
				"\n"
				"W := $(word 2,alpha beta gamma)\n"
				"J := $(join a b c,1 2)\n"
				"P := $(patsubst %.c,%.o,x.c y.c z.c)\n"
				"D := $(dir src/foo.c)\n"
				"\n"
				"$(eval EV := 42)\n"
				"\n"
				"define RULE\n"
				"hello:\n"
				"\techo hi\n"
				"endef\n"
				"$(eval $(RULE))\n";

		auto mk = Rc<MakefileRef>::create(SharedRefMode::Allocator);
		mk->setLogCallback(logcb);

		ErrorReporter err(nullptr);
		err.callback = logcb;
		err.filename = StringView("<test>");

		check(mk->include("test.mk", kFixture, true, &err), "fixture A parses");

		auto varStr = [&](StringView name) -> StringView {
			auto v = mk->getVariable(name);
			return (v && v->type == Variable::Type::String) ? v->str : StringView();
		};

		// stubbed expansion functions + refactored pattern matcher
		checkEq(varStr("W"), "beta", "$(word 2,...)");
		checkEq(varStr("J"), "a1 b2 c", "$(join a b c,1 2)");
		checkEq(varStr("P"), "x.o y.o z.o", "$(patsubst %.c,%.o,...) [refactor guard]");
		checkEq(varStr("D"), "src/", "$(dir ...) [refactor guard]");

		// $(eval) wiring: a variable assigned at runtime must take effect
		checkEq(varStr("EV"), "42", "$(eval VAR := value)");

		auto exportOne = [&](Target *t) -> String {
			String acc;
			mk->exportRecipe(t, [&](StringView line) {
				if (!acc.empty()) {
					acc.append("\n");
				}
				acc.append(line.data(), line.size());
			}, err);
			return acc;
		};

		// $(eval) of a multi-line `define` block must produce a real rule with its recipe
		// (recipe indentation must survive expansion)
		auto hello = mk->getTarget("hello");
		check(hello != nullptr, "$(eval $(define-block)) defined target 'hello'");
		if (hello) {
			auto r = exportOne(hello);
			checkEq(StringView(r.data(), r.size()), "echo hi",
					"$(eval $(define-block)) preserved the recipe");
		}

		// default goal
		auto goal = mk->getDefaultGoal();
		check(goal && goal->name == "all", "default goal is 'all'");

		// build plan: topological order, deps before dependents
		auto plan = mk->buildPlan(goal, err);
		check(!plan.empty(), "buildPlan(all) is non-empty");

		auto indexOf = [&](const Vector<BuildNode *> &p, StringView name) -> int {
			for (uint32_t i = 0; i < p.size(); ++i) {
				if (p[i]->name == name) {
					return int(i);
				}
			}
			return -1;
		};
		auto nodeOf = [&](const Vector<BuildNode *> &p, StringView name) -> BuildNode * {
			auto i = indexOf(p, name);
			return i >= 0 ? p[uint32_t(i)] : nullptr;
		};

		if (!plan.empty()) {
			check(plan.back()->name == "all", "plan goal is last (topological order)");
			auto iFooC = indexOf(plan, "foo.c");
			auto iFooO = indexOf(plan, "foo.o");
			auto iApp = indexOf(plan, "app");
			check(iFooC >= 0 && iFooO >= 0 && iFooC < iFooO, "foo.c precedes foo.o");
			check(iFooO >= 0 && iApp >= 0 && iFooO < iApp, "foo.o precedes app");

			if (auto n = nodeOf(plan, "app")) {
				auto r = exportOne(n->target);
				checkEq(StringView(r.data(), r.size()), "cc -O2 -o app foo.o bar.o",
						"app recipe ($@ $^)");
			}
			if (auto n = nodeOf(plan, "foo.o")) {
				auto r = exportOne(n->target);
				checkEq(StringView(r.data(), r.size()), "cc -O2 -c -o foo.o foo.c",
						"pattern recipe %.o:%.c ($@ $<)");
			}
		}

		// pattern rule with directories: shortest-stem selection + $* + $(@D)/$(<F)
		auto extraPlan = mk->buildPlan(mk->getTarget("extra"), err);
		if (auto n = nodeOf(extraPlan, "obj/foo.o")) {
			auto r = exportOne(n->target);
			checkEq(StringView(r.data(), r.size()),
					"echo build foo : obj/foo.o from src/foo.c [obj/foo.c]",
					"obj/%.o pattern: stem $*, $(@D), $(<F)");
		} else {
			check(false, "buildPlan(extra) resolved obj/foo.o via obj/%.o");
		}

		// out-of-date: phony targets are always rebuilt
		check(mk->isOutOfDate(goal, err), "phony target reports out of date");

		// --- fixture B: actual recipe execution via the shell ---
		static constexpr StringView kExec =
				".PHONY: all a b\n"
				"all: a b\n"
				"a:\n"
				"\t@echo running-a\n"
				"b: a\n"
				"\t@echo running-b\n";

		auto mkB = Rc<MakefileRef>::create(SharedRefMode::Allocator);
		mkB->setLogCallback(logcb);
		ErrorReporter errB(nullptr);
		errB.callback = logcb;
		errB.filename = StringView("<exec>");
		check(mkB->include("exec.mk", kExec, true, &errB), "fixture B parses");
		auto res = mkB->execute(mkB->getTarget("all"), errB);
		check(res == BuildResult::Built, "execute(all) ran phony recipes");

		// --- fixture C: real-file out-of-date check against filesystem ground truth ---
		bool wroteP = filesystem::write(FileInfo("mk_prereq", LocationCategory::AppCache),
				(const uint8_t *)"p", 1);
		auto prereqAbs = filesystem::findPath<memory::StandartInterface>(
				FileInfo("mk_prereq", LocationCategory::AppCache));
		if (wroteP && !prereqAbs.empty()) {
			auto rootDir = filepath::root(StringView(prereqAbs));
			auto mkC = Rc<MakefileRef>::create(SharedRefMode::Allocator);
			mkC->setLogCallback(logcb);
			mkC->setRootPath(rootDir);
			ErrorReporter errC(nullptr);
			errC.callback = logcb;
			errC.filename = StringView("<ood>");
			mkC->include("ood.mk", "mk_target: mk_prereq\n\t@echo build\n", true, &errC);

			filesystem::remove(FileInfo("mk_target", LocationCategory::AppCache));
			auto tgt = mkC->getTarget("mk_target");
			check(tgt && mkC->isOutOfDate(tgt, errC), "missing target file is out of date");

			filesystem::write(FileInfo("mk_target", LocationCategory::AppCache),
					(const uint8_t *)"t", 1);
			filesystem::Stat sp, st;
			bool gp = filesystem::stat(FileInfo("mk_prereq", LocationCategory::AppCache), sp);
			bool gt = filesystem::stat(FileInfo("mk_target", LocationCategory::AppCache), st);
			if (tgt && gp && gt) {
				bool expect = sp.mtime.toMicros() > st.mtime.toMicros();
				check(mkC->isOutOfDate(tgt, errC) == expect,
						"mtime comparison matches filesystem ground truth");
			}

			filesystem::remove(FileInfo("mk_prereq", LocationCategory::AppCache));
			filesystem::remove(FileInfo("mk_target", LocationCategory::AppCache));
		}

		// --- fixture D: target-specific variables (own-recipe scope) ---
		{
			static constexpr StringView kTargetVars =
					"CFLAGS := -O2\n"
					"prog : CFLAGS = -g\n"
					"prog : foo.o\n"
					"\tcc $(CFLAGS) -o $@ $^\n"
					"other : foo.o\n"
					"\tcc $(CFLAGS) -o $@ $^\n"
					"appnd : CFLAGS += -Wall\n"
					"appnd : ; echo $(CFLAGS)\n"
					"all : prog other\n";

			auto mkD = Rc<MakefileRef>::create(SharedRefMode::Allocator);
			mkD->setLogCallback(logcb);
			ErrorReporter errD(nullptr);
			errD.callback = logcb;
			errD.filename = StringView("<tvars>");
			check(mkD->include("tvars.mk", kTargetVars, true, &errD), "fixture D parses");

			auto exportD = [&](StringView name) -> String {
				String acc;
				if (auto t = mkD->getTarget(name)) {
					mkD->exportRecipe(t, [&](StringView line) {
						if (!acc.empty()) {
							acc.append("\n");
						}
						acc.append(line.data(), line.size());
					}, errD);
				}
				return acc;
			};

			auto rprog = exportD("prog");
			checkEq(StringView(rprog.data(), rprog.size()), "cc -g -o prog foo.o",
					"target-specific: prog recipe uses CFLAGS=-g");
			auto rother = exportD("other");
			checkEq(StringView(rother.data(), rother.size()), "cc -O2 -o other foo.o",
					"target-specific: other recipe uses global CFLAGS=-O2 (no leak)");
			auto rappnd = exportD("appnd");
			checkEq(StringView(rappnd.data(), rappnd.size()), "echo -O2 -Wall",
					"target-specific: appnd recipe uses CFLAGS += -Wall over global");

			// global CFLAGS must be untouched after the exports (exact restore)
			auto gcf = mkD->getVariable("CFLAGS");
			checkEq((gcf && gcf->type == Variable::Type::String) ? gcf->str : StringView(), "-O2",
					"target-specific: global CFLAGS restored to -O2 after exports");

			// storage introspection
			check(mkD->getTarget("prog") && mkD->getTarget("prog")->variables() != nullptr,
					"target-specific: prog->variables() has an entry");
			check(mkD->getTarget("all") && mkD->getTarget("all")->variables() == nullptr,
					"target-specific: pure-prereq target 'all' has no target vars");

			// external-executor API: resolve a variable as a given target sees it
			auto valInScope = [&](StringView target, StringView var) -> String {
				String acc;
				if (auto t = mkD->getTarget(target)) {
					mkD->getVariableValue(t, var,
							[&](StringView s) { acc.append(s.data(), s.size()); }, errD);
				}
				return acc;
			};
			auto vp = valInScope("prog", "CFLAGS");
			checkEq(StringView(vp.data(), vp.size()), "-g",
					"external: getVariableValue(prog, CFLAGS) == -g");
			auto vo = valInScope("other", "CFLAGS");
			checkEq(StringView(vo.data(), vo.size()), "-O2",
					"external: getVariableValue(other, CFLAGS) == -O2");

			// global query still -O2 after scoped queries (scope unwound)
			String gv;
			mkD->getVariableValue(StringView("CFLAGS"),
					[&](StringView s) { gv.append(s.data(), s.size()); }, errD);
			checkEq(StringView(gv.data(), gv.size()), "-O2",
					"external: global CFLAGS still -O2 after scoped queries");

			// withTargetScope: expand inside a target's scope
			String ws;
			mkD->withTargetScope(mkD->getTarget("prog"), [&]() {
				mkD->getVariableValue(StringView("CFLAGS"),
						[&](StringView s) { ws.append(s.data(), s.size()); }, errD);
			}, errD);
			checkEq(StringView(ws.data(), ws.size()), "-g",
					"external: withTargetScope(prog) sees CFLAGS=-g");

			// foreachTargetVariable introspection
			int tvCount = 0;
			String tvName, tvOp;
			mkD->foreachTargetVariable(mkD->getTarget("prog"),
					[&](StringView n, StringView op, const Variable &) {
				++tvCount;
				tvName.append(n.data(), n.size());
				tvOp.append(op.data(), op.size());
			});
			check(tvCount == 1 && tvName == "CFLAGS" && tvOp == "=",
					"external: foreachTargetVariable(prog) -> CFLAGS '='");
		}
	}, pool);

	memory::pool::destroy(pool);

	sprt::cout << "\nmakefile tests: failures=" << failures << "\n";
	return failures;
}

int main(int argc, const char *argv[]) {
	return perform_main(argc, argv, []() {
		// Legacy manual tests are kept commented above for reference (performThreadTests,
		// performPathTests, performTimeTests, ...).
		return performMakefileTests();
	});
}
