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
// parallel units are child processes. Each recipe command is launched through the dispatch reactor's
// process API (dispatch::Looper::spawnProcess), which runs it as a `/bin/sh -c` child with merged
// stdout/stderr, streams the output to a reader callback, and fires an exit completion (with the
// exit code) when the child terminates. Inter-process parallelism (up to the -j limit) is achieved
// by having several such children in flight at once, all multiplexed by one event loop. No worker
// threads, no ThreadPool, no blocking popen, and no hand-rolled fork/waitpid/poll machinery.

#include "Executor.h"
#include "Inspector.h"

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>

namespace xlmake {

namespace dispatch = sprt::dispatch;

namespace {

// Heap-backed (malloc, not memory-pool) storage for live job state. A pool-backed string/vector
// binds to whichever pool was active when it was constructed (AllocatorPool captures
// pool::acquire()) and keeps allocating from it for its whole life. Job::output is grown from the
// process reader callback and the recipe outlives the transient pools the reactor cycles per
// notify, so a pool binding makes the buffer hostage to a pool that can be reset under it (a
// random, timing-dependent corruption). Malloc-backed storage is pool-independent and stays valid
// for the entire recipe; Job is therefore allocated with new/delete rather than from a pool.
using JobString = sprt::__malloc_string;

template <typename Type>
using JobVector = sprt::__malloc_vector<Type>;

// A single fully-resolved recipe command line, with its decoded prefixes.
struct Command {
	// How the executor runs this line. Process spawns a `/bin/sh -c` child (the default); Write and
	// Append are in-process file writes performed via Looper::writeFile (no child) — see
	// Builder::parseWriteCommand. For the write kinds, `writePath`/`writeData` are pre-parsed and
	// `text` keeps the raw (sentinel-prefixed) line only for diagnostics.
	enum class Kind { Process, Write, Append };

	JobString text;
	Kind kind = Kind::Process;
	JobString writePath; // Write/Append: destination path
	JobString writeData; // Write/Append: content bytes (quotes stripped, trailing newline ensured)
	bool silent = false;
	bool ignoreErr = false;
	bool always = false;
	bool recursive = false; // a recursive $(MAKE) invocation: stream its output live, don't buffer
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
// sequentially (a recipe's lines are serial; parallelism is across nodes). The child is launched
// through dispatch::Looper::spawnProcess; `proc` holds the live process handle. Allocated with
// new/delete (not from a pool): its storage must not depend on a memory pool — see JobString above.
struct Job {
	NodeState *st = nullptr;
	JobVector<Command> commands;
	size_t index = 0; // current command line
	Rc<dispatch::ProcessHandle> proc; // live child for the current command line
	Rc<dispatch::FileHandle> file; // live in-process file write for the current command line
	JobString output; // buffered echo + captured bytes, flushed atomically when the target finishes
	JobString name; // display name for the progress counter (.TARGET_NAME, else the target name)
	bool hadOutput = false; // the recipe wrote to stdout/stderr (decides non-verbose suppression)
	bool failed = false; // a command failed: always show the block regardless of verbosity
	bool headerDone = false; // streaming started: counter + buffered prefix already written live
	bool lineBuffered = false; // .TARGET_BUFFER=line: stream output live, like a recursive sub-make

	// Guard for a write command: Looper::writeFile may complete synchronously (an open error fires
	// its completion before the call returns, unlike spawnProcess). While `inSyncWindow` is set the
	// completion only records the result here instead of re-entering onCommandDone — which would
	// delete this Job mid-call. spawn() then settles it after the call returns.
	bool inSyncWindow = false;
	bool cmdSettled = false;
	int cmdSyncCode = 0;
};

class Builder {
public:
	Builder(Makefile *mk, const BuildConfig &cfg, dispatch::Looper *looper, ErrorReporter &err)
	: _mk(mk), _cfg(cfg), _looper(looper), _err(err) {
		_pool = memory::pool::acquire();
		uint32_t hw = uint32_t(sprt::thread::hardware_concurrency());
		_jobLimit = _cfg.jobs ? _cfg.jobs : (hw ? hw : uint32_t(1));
		// Ninja-style progress counter: on for real builds, off for the data-extraction / quiet
		// modes (dry-run output is parsed by the VSCode extension; -s asked for silence; -p is a
		// pure dump). -q never reaches the Builder (runBuild returns earlier).
		_counter = !_cfg.dryRun && !_cfg.silent && !_cfg.printDatabase;
		// Honour the makefile's `verbose` flag (GNU `ifdef verbose`: true when defined to a
		// non-empty value). When off, a target that ran cleanly with no output of its own collapses
		// to just its counter line — its recipe echo and (empty) output are suppressed.
		_mk->getVariableValue(StringView("verbose"), [&](StringView v) {
			if (!v.empty()) {
				_verbose = true;
			}
		}, _err);

		// $(MAKE) expands to argv[0]; a recipe line whose first token is this binary is a recursive
		// sub-make whose output we stream live rather than buffer.
		_mk->getVariableValue(StringView("MAKE"),
				[&](StringView v) { _makeCommand.append(v.data(), v.size()); }, _err);
		StringView mc(_makeCommand.data(), _makeCommand.size());
		mc.trimChars<StringView::WhiteSpace>();
		if (mc.size() != _makeCommand.size()) {
			JobString trimmed;
			trimmed.assign(mc.data(), mc.size());
			_makeCommand = sp::move(trimmed);
		}

		// $(WRITE)/$(APPEND) expand to a sentinel marker; a recipe line whose leading token is one of
		// these is an in-process file write (no child process). Load the expanded values so
		// parseWriteCommand can recognize them, exactly like _makeCommand recognizes $(MAKE).
		_mk->getVariableValue(StringView("WRITE"),
				[&](StringView v) { _writeMarker.append(v.data(), v.size()); }, _err);
		_mk->getVariableValue(StringView("APPEND"),
				[&](StringView v) { _appendMarker.append(v.data(), v.size()); }, _err);
	}

	BuildResult buildGoal(Target *goal);

private:
	void resetState();
	void seed(const Vector<BuildNode *> &plan);
	void pump();
	void dispatchNode(NodeState *st);
	void spawn(Job *job);
	void onCommandDone(Job *job, int code);
	void finishNode(NodeState *st, bool success, bool rebuilt);
	void flush(Job *job);
	void emitCounter(Job *job); // print one "[depth][N/M] name" progress line, advancing _done
	void beginStream(Job *job); // start live streaming: emit counter + buffered prefix once
	JobString displayName(BuildNode *bn); // .TARGET_NAME (target scope) or the target's own name
	bool isLineBuffered(BuildNode *bn); // .TARGET_BUFFER=line (target scope): stream output live
	bool isRecursiveCommand(StringView text) const; // expanded recipe line invokes $(MAKE) directly
	// If `line` is an in-process file-write directive (leading token == the expanded $(WRITE)/$(APPEND)
	// marker), fill c.kind/writePath/writeData and return true; otherwise return false. A true return
	// with an empty writePath means the directive was malformed (no path) — spawn() reports it.
	bool parseWriteCommand(StringView line, Command &c) const;

	Makefile *_mk = nullptr;
	const BuildConfig &_cfg;
	dispatch::Looper *_looper = nullptr;
	ErrorReporter &_err;
	memory::pool_t *_pool =
			nullptr; // build pool; NodeState is allocated from it (Job is new/delete)
	uint32_t _jobLimit = 1;

	Map<BuildNode *, NodeState *> _map;
	Vector<NodeState *> _ready; // FIFO worklist (index-headed) of dispatchable nodes, in plan order
	size_t _readyHead = 0;
	uint32_t _inFlight = 0; // jobs with a live child (one slot each); completion is event-driven
	bool _failed = false;
	bool _builtAny = false;
	bool _inRun = false; // true while blocked in Looper::run(): completions may wakeup() to return
	bool _pumping = false; // guards pump() against re-entry from synchronous completions

	bool _counter = false; // emit the ninja-style [N/M] progress counter (off for dry-run/-s/-p)
	bool _verbose = false; // makefile `verbose` flag: when off, quiet targets show only the counter
	JobString
			_makeCommand; // expanded $(MAKE) (== argv[0]); a recipe starting with it is a sub-make
	JobString _writeMarker; // expanded $(WRITE); a recipe starting with it is an in-process write
	JobString _appendMarker; // expanded $(APPEND); a recipe starting with it is an in-process append
	uint32_t _total = 0; // M: recipe-running nodes for the current goal (computed before dispatch)
	uint32_t _done = 0; // N: recipes completed so far (incremented at flush, in completion order)
};

void Builder::resetState() {
	_map.clear();
	_ready.clear();
	_readyHead = 0;
	_inFlight = 0;
	_failed = false;
	_builtAny = false;
	_inRun = false;
	_pumping = false;
	_total = 0;
	_done = 0;
}

// Dispatch ready nodes until the job slots are full or the worklist drains. Synchronous
// completions (up-to-date / no-recipe / dry-run nodes) re-enter finishNode -> the worklist
// grows mid-loop, and this loop drains those too. The _pumping guard makes a re-entrant call
// (a synchronous onCommandDone reaching pump() again) a no-op so the outer loop keeps draining
// iteratively instead of recursing.
void Builder::pump() {
	if (_pumping) {
		return;
	}
	_pumping = true;
	while (_inFlight < _jobLimit && _readyHead < _ready.size() && !(_failed && !_cfg.keepGoing)) {
		dispatchNode(_ready[_readyHead++]);
	}
	_pumping = false;
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
	// cache that recipe resolution (setAutoVars/$?) reads a moment later — so call it even under
	// --always-make, which only overrides the resulting build decision.
	bool stale = _mk->isOutOfDate(bn->target, _err);
	bool build = _cfg.alwaysMake || st->needsBuild || stale;
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

	auto job = new (sprt::nothrow) Job();
	job->st = st;
	if (_counter) {
		job->name = displayName(bn);
	}
	job->lineBuffered = isLineBuffered(bn);
	_mk->exportRecipeLines(bn->target,
			[&](StringView line, bool silent, bool ignoreErr, bool always) {
		if (line.empty()) {
			return;
		}
		Command c;
		c.text.assign(line.data(), line.size());
		c.silent = silent;
		c.ignoreErr = ignoreErr;
		c.always = always;
		// An in-process file write ($(WRITE)/$(APPEND)) is never a recursive sub-make.
		if (!parseWriteCommand(line, c)) {
			c.recursive = isRecursiveCommand(line);
		}
		job->commands.emplace_back(sp::move(c));
	}, _err);

	if (job->commands.empty()) {
		// A rule-bearing node whose recipe expanded to nothing still counts toward the plan
		// (it was tallied in _total via bn->rules): flush() emits its progress line (no body)
		// and advances _done so the counter stays in lockstep with _total.
		flush(job);
		finishNode(st, true, false);
		sprt::__delete(job);
		return;
	}

	// This node now occupies a job slot until its recipe finishes (success or failure).
	// In dry-run mode spawn() completes synchronously, so the slot is released before we return.
	++_inFlight;
	spawn(job);
}

void Builder::spawn(Job *job) {
	auto &cmd = job->commands[job->index];

	if (!cmd.silent && !_cfg.silent) {
		if (cmd.kind == Command::Kind::Process) {
			job->output.append(cmd.text.data(), cmd.text.size());
		} else {
			// A write directive carries a control-char sentinel in cmd.text; echo a clean form.
			StringView verb = (cmd.kind == Command::Kind::Write) ? StringView("WRITE")
																  : StringView("APPEND");
			job->output.append(verb.data(), verb.size());
			job->output.append(" ");
			job->output.append(cmd.writePath.data(), cmd.writePath.size());
		}
		job->output.append("\n");
	}

	if (_cfg.dryRun && !cmd.always) {
		onCommandDone(job, 0); // print only; pretend success
		return;
	}

	// In-process file write ($(WRITE)/$(APPEND)): performed via the reactor's async file API — no
	// child process, no fork. onCommandDone reused unchanged (advances the recipe, frees the slot).
	if (cmd.kind != Command::Kind::Process) {
		job->proc = nullptr; // this line uses a file handle, not a child

		if (cmd.writePath.empty()) {
			job->output.append("xlmake: *** malformed $(WRITE)/$(APPEND) directive (no path)\n");
			job->failed = true;
			onCommandDone(job, -1);
			return;
		}

		auto flags = (cmd.kind == Command::Kind::Write)
				? (dispatch::OpenFlags::Write | dispatch::OpenFlags::Create
						  | dispatch::OpenFlags::Truncate)
				: (dispatch::OpenFlags::Write | dispatch::OpenFlags::Create
						  | dispatch::OpenFlags::Append);

		StringView path(cmd.writePath.data(), cmd.writePath.size());
		BytesView data(reinterpret_cast<const uint8_t *>(cmd.writeData.data()),
				cmd.writeData.size());

		// writeFile may fire its completion synchronously on an open error (then return null), unlike
		// spawnProcess. While inSyncWindow the completion only records the result instead of
		// re-entering onCommandDone (which would delete `job` mid-call); we settle it after the call.
		job->inSyncWindow = true;
		job->cmdSettled = false;
		auto h = _looper->writeFile(path, data, flags, [this, job](sprt::Status st) {
			int code = isSuccessful(st) ? 0 : -1;
			if (job->inSyncWindow) {
				job->cmdSettled = true;
				job->cmdSyncCode = code;
			} else {
				onCommandDone(job, code);
			}
		});
		job->inSyncWindow = false;

		if (job->cmdSettled) {
			onCommandDone(job, job->cmdSyncCode); // synchronous completion (e.g. open error)
			return;
		}
		if (!h) {
			sprt::cerr << "xlmake: failed to write file: " << path << "\n";
			onCommandDone(job, -1);
			return;
		}
		job->file = h; // keep the handle alive until the async completion fires
		return;
	}
	job->file = nullptr; // this line uses a child process, not a file handle

	// Stream the output live (instead of buffering it until the node completes) when either the
	// command is a recursive $(MAKE) — which emits a whole sub-build's worth of output over its
	// lifetime, whose live progress the buffer would hide — or the target opted in with
	// `.TARGET_BUFFER=line` (a long-running recipe whose incremental output should appear as it is
	// produced). Streaming emits this node's header (counter + command echo) up front, then writes
	// each complete line as it arrives. Writing only whole lines keeps the stream from tearing
	// against other concurrent nodes' atomic flushes; the trailing partial line is flushed on exit.
	bool streaming = cmd.recursive || job->lineBuffered;
	if (streaming) {
		beginStream(job);
	}

	// Launch the command through the dispatch reactor's process API. Output streams into the job's
	// buffer (flushed atomically when the target finishes); the exit completion drives onCommandDone.
	// The main thread never touches fds, fork or waitpid — the reactor owns all of that.
	job->proc = _looper->spawnProcess(cmd.text, [job, streaming](StringView bytes) {
		if (bytes.empty()) {
			return;
		}
		job->hadOutput = true;
		job->output.append(bytes.data(), bytes.size());
		if (streaming) {
			// Flush every complete line now; keep only the trailing partial line buffered.
			size_t nl = job->output.rfind('\n');
			if (nl != JobString::npos) {
				sprt::cout << StringView(job->output.data(), nl + 1);
				job->output.erase(0, nl + 1);
			}
		}
	}, [this, job](int code, sprt::Status st) {
		onCommandDone(job, isSuccessful(st) ? code : -1);
	});

	if (!job->proc) {
		sprt::cerr << "xlmake: failed to spawn command: " << cmd.text << "\n";
		onCommandDone(job, -1);
	}
}

void Builder::onCommandDone(Job *job, int code) {
	auto &cmd = job->commands[job->index];
	bool ok = (code == 0) || cmd.ignoreErr;

	if (!ok) {
		auto msg = toString("xlmake: *** [", job->st->node->name, "] error ", code, "\n");
		job->output.append(msg.data(), msg.size());
		job->failed = true; // a failed recipe is always shown, even in non-verbose mode
		flush(job);
		--_inFlight; // recipe failed: free the slot
		finishNode(job->st, false, false);
		sprt::__delete(job);
		pump(); // dispatch nodes unblocked by this completion; wake run() if the build is done
		if (_inRun && _inFlight == 0) {
			_looper->wakeup(dispatch::WakeupFlags::Graceful);
		}
		return;
	}

	++job->index;
	if (job->index < job->commands.size()) {
		spawn(job); // next line of the same recipe (keeps the same slot)
		return;
	}

	flush(job);
	--_inFlight; // recipe finished: free the slot
	_builtAny = true;
	finishNode(job->st, true, true);
	sprt::__delete(job);
	pump(); // dispatch nodes unblocked by this completion; wake run() if the build is done
	if (_inRun && _inFlight == 0) {
		_looper->wakeup(dispatch::WakeupFlags::Graceful);
	}
}

void Builder::flush(Job *job) {
	// Single-threaded: this is the only writer, so a target's whole block lands contiguously.
	// The ninja-style counter heads the block: a plain line, no carriage-return rewriting. _done
	// advances in completion order (flush is called once per built node), so the numbers are
	// monotonic and reach _total exactly.
	if (job->headerDone) {
		// A recursive sub-make already streamed its counter and every complete line; flush only the
		// trailing partial line (plus anything buffered by later, non-recursive commands of the
		// recipe). No counter here — it was emitted at stream start.
		if (!job->output.empty()) {
			sprt::cout << job->output;
			job->output.clear();
		}
		return;
	}
	if (_counter) {
		emitCounter(job);
	}
	// Non-verbose default: a target that ran cleanly with no output of its own collapses to just
	// the counter line above — its recipe echo ("rules") and (empty) output are suppressed. The
	// full block is still shown when verbose, in dry-run (listing the commands is the whole point),
	// when the recipe printed something, or when a command failed.
	bool show = _verbose || _cfg.dryRun || job->hadOutput || job->failed;
	if (show && !job->output.empty()) {
		sprt::cout << job->output;
	}
	job->output.clear();
}

// One progress line. For a recursive invocation (sub-make), the recursion depth tags the counter
// — the same [N] depth the Entering/Leaving directory lines use (xlmake[N]:) — so the interleaved
// counters of different levels stay distinguishable.
void Builder::emitCounter(Job *job) {
	++_done;
	if (_cfg.makeLevel > 0) {
		sprt::cout << "[" << _cfg.makeLevel << "][" << _done << "/" << _total << "] " << job->name
				   << "\n";
	} else {
		sprt::cout << "[" << _done << "/" << _total << "] " << job->name << "\n";
	}
}

// Begin live streaming for a job: emit its counter (once) and write out the buffered prefix (the
// command echo and anything captured before streaming began), so they head the live output.
void Builder::beginStream(Job *job) {
	if (!job->headerDone) {
		job->headerDone = true;
		if (_counter) {
			emitCounter(job);
		}
	}
	if (!job->output.empty()) {
		sprt::cout << job->output;
		job->output.clear();
	}
}

// True if an expanded recipe line invokes $(MAKE) as its leading command — i.e. its first token is
// the make binary (== argv[0]). This catches the dominant `$(MAKE) ...` / `+$(MAKE) ...` forms (the
// `+` is already stripped); a $(MAKE) buried in a compound command stays buffered, which is safe.
bool Builder::isRecursiveCommand(StringView text) const {
	if (_makeCommand.empty()) {
		return false;
	}
	while (!text.empty() && (text[0] == ' ' || text[0] == '\t')) { text = text.sub(1); }
	StringView mc(_makeCommand.data(), _makeCommand.size());
	if (text.size() < mc.size() || StringView(text.data(), mc.size()) != mc) {
		return false;
	}
	if (text.size() == mc.size()) {
		return true;
	}
	char after = text[mc.size()];
	return after == ' ' || after == '\t';
}

// Recognize and parse an in-process file-write directive. The expanded recipe line carries the
// sentinel that $(WRITE)/$(APPEND) expand to as its leading token; everything after the marker is
// `<path> <content...>`. Echo-faithful content handling: one matching layer of surrounding quotes is
// stripped and a trailing newline is ensured (added only if absent), so `$(WRITE) $@ "text"` writes
// exactly `text\n`. Returns false for an ordinary command line (left to spawnProcess).
bool Builder::parseWriteCommand(StringView line, Command &c) const {
	StringView s = line;
	while (!s.empty() && (s[0] == ' ' || s[0] == '\t')) { s = s.sub(1); }

	// A marker must be a standalone leading token (followed by whitespace or end of line). Its \x01
	// prefix makes a false match against a real command impossible.
	auto matchMarker = [&](const JobString &m) -> bool {
		if (m.empty()) {
			return false;
		}
		StringView mv(m.data(), m.size());
		if (s.size() < mv.size() || StringView(s.data(), mv.size()) != mv) {
			return false;
		}
		if (s.size() == mv.size()) {
			return true;
		}
		char after = s[mv.size()];
		return after == ' ' || after == '\t';
	};

	const JobString *marker = nullptr;
	if (matchMarker(_writeMarker)) {
		c.kind = Command::Kind::Write;
		marker = &_writeMarker;
	} else if (matchMarker(_appendMarker)) {
		c.kind = Command::Kind::Append;
		marker = &_appendMarker;
	} else {
		return false;
	}

	s = s.sub(marker->size());

	// Path: the first whitespace-delimited token after the marker.
	while (!s.empty() && (s[0] == ' ' || s[0] == '\t')) { s = s.sub(1); }
	size_t i = 0;
	while (i < s.size() && s[i] != ' ' && s[i] != '\t') { ++i; }
	StringView path = s.sub(0, i);
	c.writePath.assign(path.data(), path.size());
	if (path.empty()) {
		return true; // marker but no path: malformed — spawn() reports it
	}

	// Content: the rest of the line (separator skipped, trailing whitespace trimmed).
	s = s.sub(i);
	StringView content = s;
	content.trimChars<StringView::WhiteSpace>();

	// Strip one matching layer of surrounding quotes (so quoting can also preserve inner whitespace).
	if (content.size() >= 2) {
		char q = content[0];
		if ((q == '"' || q == '\'') && content[content.size() - 1] == q) {
			content = content.sub(1, content.size() - 2);
		}
	}

	c.writeData.assign(content.data(), content.size());
	// Ensure a trailing newline, matching `echo`/GNU make's $(file ...). Empty content thus writes a
	// single "\n"; content that already ends in a newline is left unchanged.
	if (c.writeData.empty() || c.writeData[c.writeData.size() - 1] != '\n') {
		c.writeData.append("\n", 1);
	}
	return true;
}

// The progress-line label: the target's `.TARGET_NAME` (resolved in the target's own scope, so a
// per-target assignment is honoured) when set to a non-blank value, otherwise the target's own
// name as written in the makefile.
JobString Builder::displayName(BuildNode *bn) {
	JobString raw;
	_mk->getVariableValue(bn->target, StringView(".TARGET_NAME"),
			[&](StringView v) { raw.append(v.data(), v.size()); }, _err);
	StringView val(raw.data(), raw.size());
	val.trimChars<StringView::WhiteSpace>();
	JobString out;
	if (val.empty()) {
		out.assign(bn->name.data(), bn->name.size());
	} else {
		out.assign(val.data(), val.size());
	}
	return out;
}

// A target can force live, line-buffered streaming of its recipe output — the same treatment a
// recursive $(MAKE) gets — by setting the target-specific `.TARGET_BUFFER` to `line` (resolved in the
// target's own scope, so a per-target assignment is honoured). Useful for a long-running recipe whose
// incremental progress should appear as produced rather than in one atomic block at completion. Any
// other value (or unset) keeps the default full buffering.
bool Builder::isLineBuffered(BuildNode *bn) {
	JobString raw;
	_mk->getVariableValue(bn->target, StringView(".TARGET_BUFFER"),
			[&](StringView v) { raw.append(v.data(), v.size()); }, _err);
	StringView val(raw.data(), raw.size());
	val.trimChars<StringView::WhiteSpace>();
	return val == "line";
}

BuildResult Builder::buildGoal(Target *goal) {
	resetState();

	auto plan = _mk->buildPlan(goal, _err);
	if (plan.empty()) {
		return BuildResult::Cycle;
	}
	seed(plan);

	// Progress total (M): how many nodes will actually run a recipe. Mirror the exact gate the
	// dispatch loop uses below (--always-make, or out of date, or a normal prerequisite rebuilds)
	// AND require a recipe (bn->rules) — identical to runBuild's --question pre-pass — so _done
	// reaches _total precisely. isOutOfDate force-re-stats at dispatch, so this extra stat pass
	// does not skew the real build decision.
	if (_counter) {
		Set<BuildNode *> rebuilt;
		for (auto bn : plan) {
			bool needs = _cfg.alwaysMake || _mk->isOutOfDate(bn->target, _err);
			if (!needs) {
				for (auto p : bn->prerequisites) {
					if (rebuilt.find(p) != rebuilt.end()) {
						needs = true;
						break;
					}
				}
			}
			if (needs && bn->rules) {
				++_total;
				rebuilt.emplace(bn);
			}
		}
	}

	// Event-driven build. Dispatch the initially-ready nodes, then run the reactor loop. Each
	// child's exit fires its spawnProcess completion -> onCommandDone advances the recipe, frees
	// the job slot and pump()s any freshly-unblocked nodes; when the last job finishes it wakes
	// run() so it returns. Up-to-date / no-recipe / dry-run nodes complete synchronously inside
	// pump(), so a build with no real recipes drains entirely here and never enters run(). The
	// while-loop re-enters run() defensively should it ever return with work still in flight.
	pump();
	while (_inFlight > 0) {
		_inRun = true;
		_looper->run(dispatch::TimeInterval::Infinite);
		_inRun = false;
	}

	// Let the loop finalize any pending handle cancels (process + reader sub-handles) before return.
	while (_looper->poll() > 0) { }

	if (_failed) {
		return BuildResult::Failed;
	}
	return _builtAny ? BuildResult::Built : BuildResult::UpToDate;
}

} // namespace

int runBuild(Makefile *mk, const BuildConfig &cfg, ErrorReporter &err) {
	// --print-directory: GNU make brackets the whole build with these lines; tooling uses them to
	// anchor relative paths in the dry-run output. A sub-make (MAKELEVEL > 0) tags the program name
	// with its depth — `xlmake[1]:` — exactly like GNU make's `make[1]:`; the top level is plain
	// `xlmake:`.
	auto label = cfg.makeLevel > 0 ? toString("xlmake[", cfg.makeLevel, "]") : toString("xlmake");
	if (cfg.printDirectory) {
		sprt::cout << label << ": Entering directory '" << cfg.rootDir << "'\n";
	}
	auto finish = [&](int rc) {
		if (cfg.printDirectory) {
			sprt::cout << label << ": Leaving directory '" << cfg.rootDir << "'\n";
		}
		return rc;
	};

	// --print-data-base: GNU make dumps the database after reading the makefiles, regardless of the
	// goal — emit it here so the extension gets it even for an unknown goal. Emit it BEFORE the
	// materialize-buildPlan below: that step instantiates pattern-derived nodes (foo.o, src/foo.c,
	// ...) which GNU marks `# Not a target:`; dumping first keeps getTargets() to declared targets.
	if (cfg.printDatabase) {
		printDatabase(mk, err);
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
			// No default goal. For a pure query (-p/-q) this is not a hard error.
			if (!cfg.printDatabase && !cfg.question) {
				sprt::cerr << "xlmake: *** No targets.  Stop.\n";
			}
			return finish((cfg.printDatabase && !cfg.question) ? 0 : 2);
		}
	} else {
		for (auto tn : cfg.targets) {
			if (auto t = mk->getTarget(tn)) {
				goals.emplace_back(t);
			} else {
				sprt::cerr << "xlmake: *** No rule to make target '" << tn << "'.  Stop.\n";
				return finish(2);
			}
		}
	}

	// --question: report whether `make` would run any recipe (GNU semantics: exit 0 if nothing
	// needs doing, 1 otherwise) and run nothing. Mirror the executor's per-node decision over the
	// build plan (deps first): a recipe runs for a node iff it HAS a recipe AND (--always-make, or
	// it is out of date, or a normal prerequisite would be rebuilt). The recipe gate is what makes a
	// phony aggregator like `all: app` with an up-to-date `app` report "nothing to do". No event
	// loop needed — this is pure filesystem stat.
	if (cfg.question) {
		bool wouldRun = cfg.alwaysMake;
		Set<BuildNode *> rebuilt;
		for (auto goal : goals) {
			if (wouldRun) {
				break;
			}
			for (auto bn : mk->buildPlan(goal, err)) {
				bool needs = mk->isOutOfDate(bn->target, err);
				if (!needs) {
					for (auto p : bn->prerequisites) {
						if (rebuilt.find(p) != rebuilt.end()) {
							needs = true;
							break;
						}
					}
				}
				if (needs && bn->rules) {
					wouldRun = true;
					rebuilt.emplace(bn);
				}
			}
		}
		return finish(wouldRun ? 1 : 0);
	}

	auto looper = dispatch::Looper::acquire(
			dispatch::LooperInfo{.name = StringView("xlmake"), .workersCount = 0});
	if (!looper) {
		sprt::cerr << "xlmake: failed to initialize the event loop\n";
		return finish(2);
	}

	Builder builder(mk, cfg, looper, err);
	int rc = 0;
	for (auto goal : goals) {
		auto res = builder.buildGoal(goal);
		switch (res) {
		case BuildResult::Built: break;
		case BuildResult::UpToDate:
			if (!cfg.dryRun) {
				if (cfg.makeLevel > 0) {
					sprt::cout << "xlmake[" << cfg.makeLevel << "]: " << goal->name
							   << "' is up to date\n";
				} else {

					sprt::cout << "xlmake: " << goal->name << "' is up to date\n";
				}
			}
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
	return finish(rc);
}

} // namespace xlmake
