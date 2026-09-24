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
#include <sprt/runtime/dispatch/queue.h>
#include <sprt/runtime/dispatch/handle.h>
#include <sprt/runtime/platform.h>
#include "../tests.h"

#if !SPRT_WASM && !SPRT_WINDOWS
#include <errno.h>
#include <signal.h>
#endif

namespace sprt {

// The whole suite drives real child processes, which the wasm sandbox and Embox
// have no model for; performProcessTests() reports a SKIP there instead, so none
// of the machinery below is built.
#define SPRT_TEST_NO_PROCESSES (SPRT_WASM || SPRT_EMBOX_ANY)
#if !SPRT_TEST_NO_PROCESSES

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
		sprt::cout << sprt::test::failed("FAIL  spawnProcess returned null for: ") << c.cmd << "\n";
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
	sprt::cout << (ok ? "PASS  " : sprt::test::failed("FAIL  ")) << "[" << c.cmd
			   << "] code=" << code << " (want " << c.wantCode << ") out=[" << trimmed << "]\n";
	return ok;
}

// Looper and Queue offer the same spawnProcess/wait pair, so the cancel cases run on either.
template <typename Driver>
static void driveFor(Driver *looper, uint64_t ms, const Callback<bool()> &until) {
	auto deadline = platform::clock(platform::ClockType::Monotonic) + ms * 1'000;
	while (!until() && platform::clock(platform::ClockType::Monotonic) < deadline) {
		looper->wait(dispatch::TimeInterval::milliseconds(20));
	}
}

#if !SPRT_WINDOWS
static bool isProcessAlive(int pid) { return ::kill(pid, 0) == 0 || errno != ESRCH; }

// The shell starts a grandchild and prints its pid; cancelling the handle then kills the whole
// tree with ProcessFlags::KillProcessTree, and only the shell without it.
template <typename Driver>
static bool runTreeCancelCase(Driver *looper, StringView engine, bool killTree) {
	ProcessCapture cap;
	bool done = false;
	Status finalStatus = Status::Pending;

	auto proc = looper->spawnProcess("sleep 30 & echo $!; wait",
			[&cap](StringView d) { cap.append(d); }, [&](int, Status st) {
		finalStatus = st;
		done = true;
	}, nullptr, killTree ? dispatch::ProcessFlags::KillProcessTree : dispatch::ProcessFlags::None);
	if (!proc) {
		sprt::cout << sprt::test::failed("FAIL  spawnProcess returned null (tree cancel)\n");
		return false;
	}

	driveFor(looper, 5'000, [&] { return cap.view().find('\n') != Max<size_t>; });
	auto grandchild = int(StringView(cap.view()).readInteger(10).get(0));

	proc->cancel();
	driveFor(looper, 1'000, [&] { return done; });

	// A killed orphan stays a zombie until init reaps it, so death gets a few seconds; survival
	// only needs to hold for a moment.
	bool alive = false;
	if (grandchild > 0) {
		driveFor(looper, killTree ? 3'000 : 300,
				[&] { return killTree && !isProcessAlive(grandchild); });
		alive = isProcessAlive(grandchild);
		if (alive) {
			::kill(grandchild, SIGKILL);
		}
	}

	bool ok = grandchild > 0 && done && finalStatus == Status::ErrorCancelled && alive == !killTree;
	sprt::cout << (ok ? "PASS  " : sprt::test::failed("FAIL  ")) << "[" << engine << ": cancel "
			   << (killTree ? "with" : "without") << " KillProcessTree] grandchild=" << grandchild
			   << " alive=" << (alive ? "yes" : "no") << " (want " << (killTree ? "no" : "yes")
			   << ") status=" << status::getStatusName(finalStatus) << "\n";
	return ok;
}
#endif

// Cancelling before the child has even started must neither hang nor leak.
template <typename Driver>
static bool runEarlyCancelCase(Driver *looper, StringView engine, StringView cmd) {
	bool done = false;
	Status finalStatus = Status::Pending;
	auto proc =
			looper->spawnProcess(cmd, dispatch::ProcessInfo::ReaderCallback(), [&](int, Status st) {
		finalStatus = st;
		done = true;
	}, nullptr, dispatch::ProcessFlags::KillProcessTree);
	if (!proc) {
		sprt::cout << sprt::test::failed("FAIL  spawnProcess returned null (early cancel)\n");
		return false;
	}
	proc->cancel();
	driveFor(looper, 2'000, [&] { return done; });

	bool ok = done && finalStatus == Status::ErrorCancelled;
	sprt::cout << (ok ? "PASS  " : sprt::test::failed("FAIL  ")) << "[" << engine
			   << ": early cancel '" << cmd << "'] status=" << status::getStatusName(finalStatus)
			   << "\n";
	return ok;
}

} // namespace

#endif // !SPRT_TEST_NO_PROCESSES

void performProcessTests() {
	sprt::cout << "\n== runtime process tests ==\n";

#if SPRT_WASM
	// A wasm module is a single process inside its host's sandbox: there is no
	// fork/exec, no shell to run these recipes, and no child to reap. The wasm
	// queue leaves its spawnProcess hook null on purpose (SPEvent-wasm.cc), so
	// every spawn here would return nullptr.
	sprt::cout << "SKIP  process tests (no process model in the wasm sandbox)\n";
#elif SPRT_EMBOX_ANY
	// On Embox an application does not start processes: the system does (xlexec
	// today, the window server later -- xenolith-os docs/EMBOX-USER-WM.md, §6),
	// and there is no shell for these recipes to run in. Out of scope by
	// design, not missing: said here so the group is excluded, not failed.
	sprt::cout << "SKIP  process tests (on Embox the system starts processes, not the application)\n";
#else
	auto looper = dispatch::Looper::acquire();
	if (!looper) {
		sprt::cout << sprt::test::failed("FAIL  could not acquire looper\n");
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
		int spawned = 0;
		int done = 0;
		auto before = platform::clock(platform::ClockType::Realtime);
		for (int i = 0; i < n; ++i) {
			if (looper->spawnProcess(concurrentCmd, dispatch::ProcessInfo::ReaderCallback(),
						[&](int, Status) { ++done; })) {
				++spawned;
			}
		}
		// Wait only for the children that actually started: a spawn that returned
		// nullptr never delivers a completion, so waiting for `n` of them would
		// park the loop forever with nothing left to wake it.
		while (done < spawned) { looper->wait(dispatch::TimeInterval::Infinite); }
		// platform::clock() is in microseconds
		auto elapsedMs = (platform::clock(platform::ClockType::Realtime) - before) / 1000;
		bool ok = (spawned == n) && (done == n) && elapsedMs < boundMs;
		if (!ok) {
			++failed;
		}
		sprt::cout << (ok ? "PASS  " : sprt::test::failed("FAIL  ")) << "[concurrent x" << n << " '"
				   << concurrentCmd << "'] elapsed=" << elapsedMs << "ms (want < " << boundMs
				   << ")\n";
	}

#if SPRT_WINDOWS
	if (!runEarlyCancelCase(looper, "default", "ping -n 30 127.0.0.1")) {
		++failed;
	}
#elif SPRT_LINUX
	struct EngineCase {
		StringView name;
		dispatch::QueueEngine engine;
	};

	static constexpr EngineCase s_engines[] = {
		{"uring", dispatch::QueueEngine::URing},
		{"epoll", dispatch::QueueEngine::EPoll},
	};

	for (auto &it : s_engines) {
		dispatch::QueueInfo qinfo;
		qinfo.engineMask = it.engine;
		auto queue = dispatch::Queue::create(move(qinfo));
		if (!queue || queue->getEngine() != it.engine) {
			sprt::cout << "SKIP  " << it.name << " engine is not available\n";
			continue;
		}
		failed += runEarlyCancelCase(queue.get(), it.name, "sleep 30") ? 0 : 1;
		failed += runTreeCancelCase(queue.get(), it.name, true) ? 0 : 1;
		failed += runTreeCancelCase(queue.get(), it.name, false) ? 0 : 1;
	}
#else
	failed += runEarlyCancelCase(looper, "default", "sleep 30") ? 0 : 1;
	failed += runTreeCancelCase(looper, "default", true) ? 0 : 1;
	failed += runTreeCancelCase(looper, "default", false) ? 0 : 1;
#endif

	sprt::cout << "process tests: " << (failed == 0 ? "ALL PASS" : sprt::test::failed("FAILURES"))
			   << " (failures=" << failed << ")\n";
#endif
}

} // namespace sprt
