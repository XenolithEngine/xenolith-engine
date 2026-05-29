/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_HANDLE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_HANDLE_H_

#include <sprt/runtime/dispatch/queue.h>

namespace sprt::dispatch {

class Queue;
struct QueueData;
struct HandleClass;
struct PerformEngine;

class SPRT_API alignas(32) Handle : public Ref {
public:
	static constexpr size_t DataSize = 40;

	static inline bool isValidCancelStatus(Status st) {
		return st == Status::Done || (st != Status::Declined && !isSuccessful(st));
	}

	virtual ~Handle();

	Handle();

	bool init(HandleClass *, CompletionHandle<void> &&);

	void setUserdata(Ref *ref) { _userdata = ref; }
	Ref *getUserdata() const { return _userdata; }

	// Initially, handle in Pending state
	// When handle run within queue - it's on Ok state
	// When handle completes it's execution, it's on Done state
	// When handle's execution were suspended (like on graceful wakeup) - it's on Suspended state
	// When handle is paused - it's on Declined state
	Status getStatus() const { return _status; }

	// Timeline is monotonically increased in each suspend/resume cycle for internal synchronization
	uint32_t getTimeline() const { return _timeline; }

	const HandleClass *getClass() const { return _class; }

	bool isResumable() const;

	// Pause handle's execution, handle should be resumable
	// You need to cancel paused handle in order to really stop and release it
	Status pause();

	// Resume handle's execution
	Status resume();

	// Cancel handle operation (performed asynchronically)
	// Returns:
	// Status::ErrorInvalidArguemnt - on error
	// Status::ErrorAlreadyPerformed - handle is not running
	// Status::ErrorNotPermitted - handle is not suspendable, only Queue can stop it
	// Status::ErrorCancelled - handle was reset to continue running by CompletionHandle
	// Status::Ok - cancel request was scheduled on queue
	Status cancel(Status = Status::ErrorCancelled, uint32_t value = 0);

protected:
	friend class Queue;
	friend struct QueueData;
	friend struct HandleClass;

#if ANDROID
	friend struct ALooperData;
#endif

	Status run();
	Status suspend();

	Status prepareRearm();
	Status prepareDisarm();

	void sendCompletion(uint32_t value, Status);
	bool finalize(uint32_t value, Status);

	bool reset();

	// platform data block
	alignas(void *) uint8_t _data[DataSize];

	HandleClass *_class = nullptr;
	CompletionHandle<void> _completion;
	Status _status = Status::Pending;
	uint32_t _timeline = 0;
	Rc<Ref> _userdata;
};

class SPRT_API PollHandle : public Handle {
public:
	virtual ~PollHandle() = default;

	virtual NativeHandle getNativeHandle() const = 0;

	virtual bool reset(PollFlags) = 0;
};

class SPRT_API TimerHandle : public Handle {
public:
	static constexpr uint32_t Infinite = TimerInfo::Infinite;

	virtual ~TimerHandle() = default;

	// Some timers can not be reset with some other params
	// like, URing-native timer can only be oneshot or infinite timer
	// If 'completion' is not specified - it stays the same
	// If 'completion' is specified - userdata will be cleared
	//
	// Be careful, when resetting timers with Function callback
	//  - this callback will become invalid when new 'completion' set
	virtual bool reset(TimerInfo &&) = 0;
};

class SPRT_API ThreadHandle : public Handle, public PerformInterface {
public:
	virtual ~ThreadHandle();

	bool init(HandleClass *);

	// resume event processing for a nested run
	virtual void wakeup();

	// Perform Task's complete functions on this event queue
	virtual Status perform(Rc<Task> &&task) override { return Status::ErrorNotImplemented; }

	// Perform function on this event queue
	virtual Status perform(dispatch::Function<void()> &&func, Ref *target = nullptr,
			StringView tag = __SPRT_FUNC) override {
		return Status::ErrorNotImplemented;
	}

protected:
	uint32_t performAll(const Callback<void(uint32_t)> &unlockCallback);

	struct CallbackInfo {
		dispatch::Function<void()> fn;
		Rc<Ref> ref;
		StringView tag;
	};

	Rc<PoolRef> _pool;
	PerformEngine *_engine = nullptr;
	dispatch::Vector<Rc<Task>> _outputQueue;
	dispatch::Vector<CallbackInfo> _outputCallbacks;

	dispatch::Vector<Rc<Task>> _unsafeQueue;
	dispatch::Vector<CallbackInfo> _unsafeCallbacks;

	uint64_t _switchTimer = 0;
};

} // namespace sprt::dispatch

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_HANDLE_H_ */
