/**
Copyright (c) 2016-2019 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_THREADS_SPTHREADTASK_H_
#define STAPPLER_THREADS_SPTHREADTASK_H_

#include "SPMemPriorityQueue.h"
#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::thread {

enum class TaskState {
	Initial,
	Prepared,
	ExecutedSuccessful,
	ExecutedFailed,
	CompletedSuccessful,
	CompletedFailed,
};

class Task;

class SP_PUBLIC TaskGroup : public Ref {
public:
	virtual ~TaskGroup() = default;

	bool init(mem_std::Function<void(const TaskGroup &, const Task &)> &&);

	virtual void handleAdded(const Task *);
	virtual void handleCompleted(const Task *);

	Pair<size_t, size_t> getCounters() const; // <completed, added>

protected:
	sprt::atomic<size_t> _added = 0;
	sprt::atomic<size_t> _completed = 0;

	mem_std::Function<void(const TaskGroup &, const Task &)> _notifyFn;
};

class SP_PUBLIC Task : public Ref {
public:
	static const uint32_t INVALID_TAG = sprt::Max<uint32_t>;

	/* Function to be executed in init phase */
	using PrepareCallback = mem_std::Function<bool(const Task &)>;

	/* Function to be executed in other thread */
	using ExecuteCallback = mem_std::Function<bool(const Task &)>;

	/* Function to be executed after task is performed */
	using CompleteCallback = mem_std::Function<void(const Task &, bool)>;

	using PriorityType =
			ValueWrapper<memory::PriorityQueue<Rc<Task>>::PriorityType, class PriorityTypeFlag>;

	virtual ~Task() = default;

	/* creates empty task with only complete function to be used as callback from other thread */
	bool init(const CompleteCallback &, Ref * = nullptr, TaskGroup * = nullptr,
			StringView tag = SP_FUNC);
	bool init(CompleteCallback &&, Ref * = nullptr, TaskGroup * = nullptr,
			StringView tag = SP_FUNC);

	/* creates regular async task without initialization phase */
	bool init(const ExecuteCallback &, const CompleteCallback & = nullptr, Ref * = nullptr,
			TaskGroup * = nullptr, StringView tag = SP_FUNC);
	bool init(ExecuteCallback &&, CompleteCallback && = nullptr, Ref * = nullptr,
			TaskGroup * = nullptr, StringView tag = SP_FUNC);

	/* creates regular async task with initialization phase */
	bool init(const PrepareCallback &, const ExecuteCallback &, const CompleteCallback & = nullptr,
			Ref * = nullptr, TaskGroup * = nullptr, StringView tag = SP_FUNC);
	bool init(PrepareCallback &&, ExecuteCallback &&, CompleteCallback && = nullptr,
			Ref * = nullptr, TaskGroup * = nullptr, StringView tag = SP_FUNC);

	/* adds one more function to be executed before task is added to queue, functions executed as FIFO */
	void addPrepareCallback(const PrepareCallback &);
	void addPrepareCallback(PrepareCallback &&);

	/* adds one more function to be executed in other thread, functions executed as FIFO */
	void addExecuteCallback(const ExecuteCallback &);
	void addExecuteCallback(ExecuteCallback &&);

	/* adds one more function to be executed when task is performed, functions executed as FIFO */
	void addCompleteCallback(const CompleteCallback &);
	void addCompleteCallback(CompleteCallback &&);

	/* mark this task with tag */
	void setTag(StringView tag) { _tag = tag; }

	/* returns tag */
	StringView getTag() const { return _tag; }

	/* set default task priority */
	void setPriority(PriorityType::Type priority) { _priority = PriorityType(priority); }

	/* get task priority */
	PriorityType getPriority() const { return _priority; }

	TaskGroup *getGroup() const { return _group; }

	void addRef(Ref *target) {
		if (target) {
			_refs.emplace_back(target);
		}
	}

	/* if task execution was successful */
	bool isSuccessful() const {
		return _state == TaskState::ExecutedSuccessful || _state == TaskState::CompletedSuccessful;
	}

	const mem_std::Vector<PrepareCallback> &getPrepareTasks() const { return _prepare; }
	const mem_std::Vector<ExecuteCallback> &getExecuteTasks() const { return _execute; }
	const mem_std::Vector<CompleteCallback> &getCompleteTasks() const { return _complete; }

public:
	// run in one call on current thread
	void run() const;

	/** called on issuer thread before execution */
	virtual bool prepare() const;

	/** called on worker thread */
	virtual bool execute() const;

	/** called on dispatcher thread when request is completed */
	virtual void handleCompleted() const;

	/** called when assigned worker can not perform the task */
	virtual void cancel() const;

protected:
	// prepare/execute/handleCompleted marked as const to forbid task changes, but it can change state
	mutable TaskState _state = TaskState::Initial;
	StringView _tag;
	PriorityType _priority = PriorityType();

	mem_std::Vector<Rc<Ref>> _refs;
	mem_std::Vector<PrepareCallback> _prepare;
	mem_std::Vector<ExecuteCallback> _execute;
	mem_std::Vector<CompleteCallback> _complete;
	Rc<TaskGroup> _group;
};

} // namespace stappler::thread

#endif /* STAPPLER_THREADS_SPTHREADTASK_H_ */
