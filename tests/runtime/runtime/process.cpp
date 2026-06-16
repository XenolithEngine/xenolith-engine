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

// Tests for dispatch::Looper::spawnProcess: child output streaming and exit-code
// reporting through the reactor's process backend (pidfd on Linux, EVFILT_PROC on
// macOS, IOCP on Windows). The recipes use POSIX shell syntax, matching the rest
// of this suite's POSIX libc tests.

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>
#include <sprt/runtime/platform.h>

namespace sprt {

namespace {

namespace dispatch = sprt::dispatch;

struct ProcessCase {
	StringView cmd;
	StringView wantContains;
	int wantCode;
};

// Output is captured into a fixed buffer (no pool/heap dependency from inside the
// looper callbacks, which run within a transient notify pool).
struct ProcessCapture {
	char buf[8192];
	size_t len = 0;
	void append(StringView d) {
		size_t n = d.size();
		if (n > sizeof(buf) - len) {
			n = sizeof(buf) - len;
		}
		if (n) {
			sprt::memcpy(buf + len, d.data(), n);
			len += n;
		}
	}
	StringView view() const { return StringView(buf, len); }
};

static bool runProcessCase(dispatch::Looper *looper, const ProcessCase &c) {
	ProcessCapture cap;
	bool done = false;
	int code = -999;
	Status finalStatus = Status::Pending;

	auto proc = looper->spawnProcess(c.cmd, [&cap](StringView d) { cap.append(d); },
			[&](int ec, Status st) {
		code = ec;
		finalStatus = st;
		done = true;
	});

	if (!proc) {
		sprt::cout << "FAIL  spawnProcess returned null for: " << c.cmd << "\n";
		return false;
	}

	// Drive the loop until the exit completion fires.
	while (!done) { looper->wait(dispatch::TimeInterval::Infinite); }

	auto out = cap.view();
	bool codeOk = (code == c.wantCode);
	bool outOk = c.wantContains.empty() || out.find(c.wantContains) != Max<size_t>;
	bool ok = codeOk && outOk && isSuccessful(finalStatus);

	StringView trimmed = out;
	trimmed.trimChars<StringView::WhiteSpace>();
	sprt::cout << (ok ? "PASS  " : "FAIL  ") << "[" << c.cmd << "] code=" << code << " (want "
			   << c.wantCode << ") out=[" << trimmed << "]\n";
	return ok;
}

} // namespace

void performProcessTests() {
	sprt::cout << "\n== runtime process tests ==\n";

	auto looper = dispatch::Looper::acquire();
	if (!looper) {
		sprt::cout << "FAIL  could not acquire looper\n";
		return;
	}

	// Commands are shell-specific: POSIX /bin/sh on Linux/macOS, cmd.exe on Windows.
#if SPRT_WINDOWS
	ProcessCase cases[] = {
		{"echo hi& exit 7", "hi", 7}, // stdout capture + exit code (& is cmd's separator)
		{"echo one& echo two", "two", 0}, // multiple writes
		{"echo err 1>&2& exit 3", "err", 3}, // stderr is merged
		{"exit 0", StringView(), 0}, // no output
		{"exit 9", StringView(), 9}, // exit code only
	};
#else
	ProcessCase cases[] = {
		{"printf hi; exit 7", "hi", 7}, // stdout capture + exit code
		{"echo one; sleep 0.2; echo two", "two", 0}, // streaming across loop wakeups
		{"echo to-stderr 1>&2; exit 3", "to-stderr", 3}, // stderr is merged
		{"exit 0", StringView(), 0}, // no output
		{"kill -TERM $$", StringView(), 128 + 15}, // killed by signal -> 128 + SIGTERM
	};
#endif

	int failed = 0;
	for (auto &c : cases) {
		if (!runProcessCase(looper, c)) {
			++failed;
		}
	}

	// Concurrency: many children in flight at once, all multiplexed by one loop. On POSIX they
	// sleep, so finishing in ~max(single) rather than the sum proves real parallelism; cmd.exe has
	// no portable sub-second sleep, so on Windows this just exercises concurrent completion delivery.
#if SPRT_WINDOWS
	StringView concurrentCmd = "exit 0";
	auto boundMs = 5000;
#else
	StringView concurrentCmd = "sleep 0.3";
	auto boundMs = 1500; // 8x 0.3s serial would be ~2.4s
#endif
	{
		int n = 8;
		int done = 0;
		auto before = platform::clock(platform::ClockType::Realtime);
		for (int i = 0; i < n; ++i) {
			looper->spawnProcess(concurrentCmd, dispatch::ProcessInfo::ReaderCallback(),
					[&](int, Status) { ++done; });
		}
		while (done < n) { looper->wait(dispatch::TimeInterval::Infinite); }
		// platform::clock() is in microseconds
		auto elapsedMs = (platform::clock(platform::ClockType::Realtime) - before) / 1000;
		bool ok = (done == n) && elapsedMs < boundMs;
		if (!ok) {
			++failed;
		}
		sprt::cout << (ok ? "PASS  " : "FAIL  ") << "[concurrent x" << n << " '" << concurrentCmd
				   << "'] elapsed=" << elapsedMs << "ms (want < " << boundMs << ")\n";
	}

	sprt::cout << "process tests: " << (failed == 0 ? "ALL PASS" : "FAILURES") << " (failures="
			   << failed << ")\n";
}

} // namespace sprt
