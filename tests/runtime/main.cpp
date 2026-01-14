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
#include "SPTime.h"

#include "SPFilesystem.h"
#include "SPThread.h"

#include "SPData.h"
#include "SPDataValue.h"

#include <sprt/runtime/backtrace.h>
#include <sprt/runtime/platform.h>
#include <sprt/runtime/compress.h>
#include <sprt/runtime/idn.h>

using namespace stappler;

static sprt::qmutex s_mutex;

class TestThread : public thread::Thread {
public:
	virtual void threadInit() override {
		sprt::unique_lock lock(s_mutex);
		Thread::threadInit();
		slog().debug("Thread", "threadInit");
	}
	virtual void threadDispose() override {
		sprt::unique_lock lock(s_mutex);
		Thread::threadDispose();
		slog().debug("Thread", "threadDispose");
	}
	virtual bool worker() override {
		sprt::unique_lock lock(s_mutex);
		slog().debug("Thread", "worker");
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
}

static void performDynAllocTests() {
	sprt::memory::dynstring str;
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
	StringView test1 = "Тест1";
	StringView test2 = "ТЕСТ1";
	StringView test3 = "ТЕСТ3asd";

	WideStringView wtest1 = u"Тест1";
	WideStringView wtest2 = u"ТЕСТ1";

	std::cout << platform::toupper<memory::StandartInterface>(test1) << "\n";
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
}

int main(int argc, const char *argv[]) {
	return perform_main(argc, argv, []() {
		//printCaseTables();
		//runDataCoverter();

		performIdnTests();
		performThreadTests();
		performDynAllocTests();
		performPathTests();
		performTimeTests();
		performUnicodeTests();

		sprt::backtrace::getBacktrace(0, [](StringView str) { std::cout << str << "\n"; });

		return 0;
	});
}
