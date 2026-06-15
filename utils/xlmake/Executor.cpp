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

// xlmake build executor: a single-threaded, non-blocking build reactor.
//
// The main thread does ALL makefile work (build plan, recipe resolution, stat, printing); the only
// parallel units are child processes. Each recipe command runs as a `/bin/sh -c` child whose merged
// stdout/stderr is a non-blocking pipe registered with the dispatch event loop as a pollable handle.
// The loop wakes when a child writes or its pipe reaches EOF (the child exited); the main thread then
// drains the pipe, reaps the child, and advances the schedule. Inter-process parallelism (up to the
// -j limit) is achieved by having several such children in flight at once, all multiplexed by one
// event loop. No worker threads, no ThreadPool, no blocking popen.

#include "Executor.h"

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// The sprt freestanding libc does not expose <sys/wait.h> on its include path, but the underlying C
// library provides waitpid(); declare the little we use directly (prototype + status-decode macros).
extern "C" pid_t waitpid(pid_t __pid, int *__stat_loc, int __options);

#ifndef WNOHANG
#define WNOHANG 1
#endif
#ifndef WIFEXITED
#define WIFEXITED(status) (((status) & 0x7f) == 0)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(status) (((status) & 0xff00) >> 8)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(status) (((signed char)(((status) & 0x7f) + 1) >> 1) > 0)
#endif
#ifndef WTERMSIG
#define WTERMSIG(status) ((status) & 0x7f)
#endif

namespace xlmake {

namespace dispatch = sprt::dispatch;

namespace {

// A single fully-resolved recipe command line, with its decoded prefixes.
struct Command {
	String text;
	bool silent = false;
	bool ignoreErr = false;
	bool always = false;
};

struct NodeState;

// A reverse edge prereq -> dependent. `normalEdge` is false for order-only edges, which gate
// scheduling order but never propagate a rebuild (cascade) or appear in automatic variables.
struct Dependent {
	NodeState *node = nullptr;
	bool normalEdge = true;
};

// Per-plan-node scheduling state, parallel to the BuildNode graph.
struct NodeState : AllocBase {
	BuildNode *node = nullptr;
	uint32_t pending = 0; // unmet prerequisites (normal + order-only), counted once per unique node
	Vector<Dependent> dependents; // reverse edges
	bool queued = false; // already placed on the ready worklist / dispatched
	bool needsBuild = false; // a normal prerequisite was rebuilt this run (cascade)
	bool prereqFailed = false; // a prerequisite failed: this node must be skipped
};

// One running recipe: a job owns at most one live child at a time and walks its command list
// sequentially (a recipe's lines are serial; parallelism is across nodes).
struct Job : AllocBase {
	NodeState *st = nullptr;
	Vector<Command> commands;
	size_t index = 0; // current command line
	pid_t pid = -1;
	int readFd = -1; // child's merged stdout/stderr pipe, non-blocking
	Rc<dispatch::PollHandle> poll;
	String output; // buffered echo + captured bytes, flushed atomically when the target finishes
	bool eof = false;
};

class Builder {
public:
	Builder(Makefile *mk, const BuildConfig &cfg, dispatch::Looper *looper, ErrorReporter &err)
	: _mk(mk), _cfg(cfg), _looper(looper), _err(err) {
		_pool = memory::pool::acquire();
		uint32_t hw = uint32_t(sprt::thread::hardware_concurrency());
		_jobLimit = _cfg.jobs ? _cfg.jobs : (hw ? hw : uint32_t(1));
	}

	BuildResult buildGoal(Target *goal);

private:
	void resetState();
	void seed(const Vector<BuildNode *> &plan);
	void dispatchNode(NodeState *st);
	void spawn(Job *job);
	sprt::Status onPoll(Job *job);
	void drain(Job *job);
	void reapRunning();
	void onCommandDone(Job *job, int code);
	void finishNode(NodeState *st, bool success, bool rebuilt);
	void flush(Job *job);

	Makefile *_mk = nullptr;
	const BuildConfig &_cfg;
	dispatch::Looper *_looper = nullptr;
	ErrorReporter &_err;
	memory::pool_t *_pool = nullptr; // build pool; NodeState/Job are allocated from it
	uint32_t _jobLimit = 1;

	Map<BuildNode *, NodeState *> _map;
	Vector<NodeState *> _ready; // FIFO worklist (index-headed) of dispatchable nodes, in plan order
	size_t _readyHead = 0;
	Vector<Job *> _running; // jobs with a live child process (drained via poll, reaped via waitpid)
	bool _failed = false;
	bool _builtAny = false;
};

void Builder::resetState() {
	_map.clear();
	_ready.clear();
	_readyHead = 0;
	_running.clear();
	_failed = false;
	_builtAny = false;
}

void Builder::seed(const Vector<BuildNode *> &plan) {
	for (auto bn : plan) {
		auto st = new (_pool) NodeState();
		st->node = bn;
		_map.emplace(bn, st);
	}

	// reverse edges + pending counts (dedup per node: a target listed twice, or as both a normal
	// and an order-only prerequisite, must be counted once so its single completion unblocks us).
	// Iterate the plan (topological order) so dependent lists and the ready queue are deterministic.
	for (auto bn : plan) {
		auto st = _map.find(bn)->second;
		Set<BuildNode *> seen;
		for (auto p : bn->prerequisites) {
			if (!seen.emplace(p).second) {
				continue;
			}
			_map.find(p)->second->dependents.emplace_back(Dependent{st, true});
			++st->pending;
		}
		for (auto p : bn->orderOnly) {
			if (!seen.emplace(p).second) {
				continue; // already a normal prerequisite: the normal edge wins
			}
			_map.find(p)->second->dependents.emplace_back(Dependent{st, false});
			++st->pending;
		}
	}

	for (auto bn : plan) {
		auto st = _map.find(bn)->second;
		if (st->pending == 0) {
			st->queued = true;
			_ready.emplace_back(st);
		}
	}
}

void Builder::finishNode(NodeState *st, bool success, bool rebuilt) {
	if (!success) {
		_failed = true;
	}
	for (auto &d : st->dependents) {
		if (!success) {
			d.node->prereqFailed = true;
		} else if (rebuilt && d.normalEdge) {
			d.node->needsBuild = true;
		}
		if (--d.node->pending == 0 && !d.node->queued) {
			d.node->queued = true;
			_ready.emplace_back(d.node);
		}
	}
}

void Builder::dispatchNode(NodeState *st) {
	auto bn = st->node;

	// A prerequisite could not be made: skip this node and propagate the failure to its dependents.
	if (st->prereqFailed) {
		finishNode(st, false, false);
		return;
	}

	// isOutOfDate() force-stats the target and its direct prerequisites, which also refreshes the
	// cache that recipe resolution (setAutoVars/$?) reads a moment later.
	bool stale = _mk->isOutOfDate(bn->target, _err);
	bool build = st->needsBuild || stale;
	if (!build) {
		finishNode(st, true, false); // up to date
		return;
	}

	if (!bn->rules) {
		// nothing to run: ok if the file exists or the target is phony, otherwise no rule to make it
		if (!bn->phony && !bn->target->fileExists) {
			sprt::cerr << "xlmake: *** No rule to make target '" << bn->name << "'\n";
			finishNode(st, false, false);
			return;
		}
		finishNode(st, true, false);
		return;
	}

	auto job = new (_pool) Job();
	job->st = st;
	_mk->exportRecipeLines(bn->target,
			[&](StringView line, bool silent, bool ignoreErr, bool always) {
		if (line.empty()) {
			return;
		}
		Command c;
		c.text = line.str<memory::PoolInterface>();
		c.silent = silent;
		c.ignoreErr = ignoreErr;
		c.always = always;
		job->commands.emplace_back(sp::move(c));
	}, _err);

	if (job->commands.empty()) {
		finishNode(st, true, false);
		return;
	}

	spawn(job);
}

void Builder::spawn(Job *job) {
	auto &cmd = job->commands[job->index];

	if (!cmd.silent && !_cfg.silent) {
		job->output.append(cmd.text.data(), cmd.text.size());
		job->output.append("\n");
	}

	if (_cfg.dryRun && !cmd.always) {
		onCommandDone(job, 0); // print only; pretend success
		return;
	}

	int fds[2];
	if (::pipe(fds) != 0) {
		sprt::cerr << "xlmake: pipe() failed\n";
		onCommandDone(job, -1);
		return;
	}

	// Parent keeps the read end (non-blocking, close-on-exec). The write end is close-on-exec so the
	// extra parent/child copies vanish at exec; only the child's dup'd stdout/stderr remain, so the
	// pipe reaches EOF exactly when the command (and any process group it keeps in the foreground)
	// closes them.
	::fcntl(fds[0], F_SETFD, FD_CLOEXEC);
	::fcntl(fds[0], F_SETFL, ::fcntl(fds[0], F_GETFL) | O_NONBLOCK);
	::fcntl(fds[1], F_SETFD, FD_CLOEXEC);

	pid_t pid = ::fork();
	if (pid < 0) {
		::close(fds[0]);
		::close(fds[1]);
		sprt::cerr << "xlmake: fork() failed\n";
		onCommandDone(job, -1);
		return;
	}
	if (pid == 0) {
		// child: merge stdout+stderr onto the pipe and exec the shell (async-signal-safe path only)
		::dup2(fds[1], STDOUT_FILENO);
		::dup2(fds[1], STDERR_FILENO);
		::execl("/bin/sh", "sh", "-c", cmd.text.data(), (char *)nullptr);
		::_exit(127);
	}

	::close(fds[1]);
	job->pid = pid;
	job->readFd = fds[0];
	job->eof = false;
	// The pollable handle keeps the pipe promptly drained (so a chatty recipe never blocks on a
	// full pipe); it is NOT relied on for completion. Completion is detected by waitpid() in
	// reapRunning(), which the loop runs on every wakeup — robust even if a poll edge is missed.
	job->poll = _looper->listenPollableHandle(dispatch::NativeHandle(fds[0]),
			dispatch::PollFlags::In | dispatch::PollFlags::HungUp,
			[this, job](dispatch::NativeHandle, dispatch::PollFlags) -> sprt::Status {
		return onPoll(job);
	});
	_running.emplace_back(job);
}

void Builder::drain(Job *job) {
	if (job->readFd < 0) {
		return;
	}
	char buf[4_KiB];
	for (;;) {
		ssize_t n = ::read(job->readFd, buf, sizeof(buf));
		if (n > 0) {
			job->output.append(buf, size_t(n));
			continue;
		}
		if (n == 0) {
			job->eof = true; // all write ends closed: the child finished writing
			return;
		}
		if (errno == EINTR) {
			continue;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return; // no more data right now; wait for the next event / sweep
		}
		job->eof = true; // unexpected read error: treat as finished
		return;
	}
}

sprt::Status Builder::onPoll(Job *job) {
	drain(job);
	// Stop polling once the pipe hits EOF (child finished writing); reapRunning() will waitpid()
	// the child and run completion. A non-Ok return cancels this handle.
	return job->eof ? sprt::Status::Done : sprt::Status::Ok;
}

// Reap every running child that has exited (non-blocking), flushing its output first, then run its
// completion. Driven from the main loop on every wakeup, so completion never depends on a poll
// event being delivered for the pipe's EOF.
void Builder::reapRunning() {
	if (_running.empty()) {
		return;
	}
	Vector<Job *> finished;
	Vector<int> codes;
	Vector<Job *> still;
	for (auto job : _running) {
		drain(job); // pull buffered output (keeps the pipe from filling)
		int status = 0;
		pid_t r = ::waitpid(job->pid, &status, WNOHANG);
		if (r == job->pid) {
			drain(job); // final flush of anything written just before exit
			if (job->readFd >= 0) {
				::close(job->readFd);
				job->readFd = -1;
			}
			if (job->poll) {
				job->poll->cancel();
				job->poll = nullptr;
			}
			int code;
			if (WIFEXITED(status)) {
				code = WEXITSTATUS(status);
			} else if (WIFSIGNALED(status)) {
				code = 128 + WTERMSIG(status);
			} else {
				code = -1;
			}
			finished.emplace_back(job);
			codes.emplace_back(code);
		} else {
			still.emplace_back(job);
		}
	}
	// Replace the running set before invoking completions: onCommandDone() may spawn the next
	// recipe line, which pushes a fresh job onto _running.
	_running = sp::move(still);
	for (size_t i = 0; i < finished.size(); ++i) { onCommandDone(finished[i], codes[i]); }
}

void Builder::onCommandDone(Job *job, int code) {
	auto &cmd = job->commands[job->index];
	bool ok = (code == 0) || cmd.ignoreErr;

	if (!ok) {
		job->output.append(toString("xlmake: *** [", job->st->node->name, "] error ", code, "\n"));
		flush(job);
		finishNode(job->st, false, false);
		return;
	}

	++job->index;
	if (job->index < job->commands.size()) {
		spawn(job); // next line of the same recipe (re-enters _running)
		return;
	}

	flush(job);
	_builtAny = true;
	finishNode(job->st, true, true);
}

void Builder::flush(Job *job) {
	// Single-threaded: this is the only writer, so a target's whole block lands contiguously.
	if (!job->output.empty()) {
		sprt::cout << job->output;
		job->output.clear();
	}
}

BuildResult Builder::buildGoal(Target *goal) {
	resetState();

	auto plan = _mk->buildPlan(goal, _err);
	if (plan.empty()) {
		return BuildResult::Cycle;
	}
	seed(plan);

	// A periodic timer wakes the loop even when no child produces output, so reapRunning() runs
	// regularly. Using a real timer event (rather than a wait() timeout) avoids the io_uring
	// backend logging every timed wait as an error. The callback is a no-op: the wake is enough.
	dispatch::TimerInfo ti;
	ti.completion = dispatch::TimerInfo::Completion::create<Builder>(this,
			[](Builder *, dispatch::TimerHandle *, uint32_t, sprt::Status) { });
	ti.timeout = dispatch::TimeInterval::milliseconds(20);
	ti.interval = dispatch::TimeInterval::milliseconds(20);
	ti.count = dispatch::TimerInfo::Infinite;
	auto timer = _looper->scheduleTimer(sp::move(ti));

	for (;;) {
		while (_running.size() < _jobLimit && _readyHead < _ready.size()
				&& !(_failed && !_cfg.keepGoing)) {
			dispatchNode(_ready[_readyHead++]);
		}
		if (_running.empty()) {
			break; // nothing in flight and nothing left to dispatch
		}
		// Wake on child output (prompt pipe draining) or the periodic timer, then reap exited
		// children. The timer guarantees forward progress even if a pollable-fd EOF event is missed.
		_looper->wait(dispatch::TimeInterval::Infinite);
		reapRunning();
	}

	if (timer) {
		timer->cancel();
	}
	// Let the loop finalize any pending handle cancels before we return.
	while (_looper->poll() > 0) { }

	if (_failed) {
		return BuildResult::Failed;
	}
	return _builtAny ? BuildResult::Built : BuildResult::UpToDate;
}

} // namespace

int runBuild(Makefile *mk, const BuildConfig &cfg, ErrorReporter &err) {
	auto looper = dispatch::Looper::acquire(
			dispatch::LooperInfo{.name = StringView("xlmake"), .workersCount = 0});
	if (!looper) {
		sprt::cerr << "xlmake: failed to initialize the event loop\n";
		return 2;
	}

	// Materialize pattern-derived targets so explicit goals are queryable by name.
	if (auto g = mk->getDefaultGoal()) {
		mk->buildPlan(g, err);
	}

	Vector<Target *> goals;
	if (cfg.targets.empty()) {
		if (auto g = mk->getDefaultGoal()) {
			goals.emplace_back(g);
		} else {
			sprt::cerr << "xlmake: no target given and no default goal\n";
			return 2;
		}
	} else {
		for (auto tn : cfg.targets) {
			if (auto t = mk->getTarget(tn)) {
				goals.emplace_back(t);
			} else {
				sprt::cerr << "xlmake: unknown target: " << tn << "\n";
				return 2;
			}
		}
	}

	Builder builder(mk, cfg, looper, err);
	int rc = 0;
	for (auto goal : goals) {
		auto res = builder.buildGoal(goal);
		switch (res) {
		case BuildResult::Built: break;
		case BuildResult::UpToDate:
			sprt::cout << "xlmake: '" << goal->name << "' is up to date\n";
			break;
		case BuildResult::Failed: rc = 2; break;
		case BuildResult::Cycle:
			sprt::cerr << "xlmake: dependency cycle involving '" << goal->name << "'\n";
			rc = 2;
			break;
		case BuildResult::NoRule: rc = 2; break;
		}
		if (rc != 0 && !cfg.keepGoing) {
			break;
		}
	}
	return rc;
}

} // namespace xlmake
