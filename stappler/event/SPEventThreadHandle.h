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

#ifndef CORE_EVENT_SPEVENTTHREADHANDLE_H_
#define CORE_EVENT_SPEVENTTHREADHANDLE_H_

#include "SPEventHandle.h"
#include "SPThreadPool.h"

namespace STAPPLER_VERSIONIZED stappler::event {

struct PerformEngine;

class SP_PUBLIC ThreadHandle : public Handle, public thread::PerformInterface {
public:
	virtual ~ThreadHandle();

	bool init(HandleClass *);

	// resume event processing for a nested run
	virtual void wakeup();

	// Perform Task's complete functions on this event queue
	virtual Status perform(Rc<thread::Task> &&task) override { return Status::ErrorNotImplemented; }

	// Perform function on this event queue
	virtual Status perform(mem_std::Function<void()> &&func, Ref *target = nullptr,
			StringView tag = SP_FUNC) override {
		return Status::ErrorNotImplemented;
	}

protected:
	uint32_t performAll(const Callback<void(uint32_t)> &unlockCallback);

	struct CallbackInfo {
		mem_std::Function<void()> fn;
		Rc<Ref> ref;
		StringView tag;
	};

	Rc<PoolRef> _pool;
	PerformEngine *_engine = nullptr;
	mem_std::Vector<Rc<thread::Task>> _outputQueue;
	mem_std::Vector<CallbackInfo> _outputCallbacks;

	mem_std::Vector<Rc<thread::Task>> _unsafeQueue;
	mem_std::Vector<CallbackInfo> _unsafeCallbacks;

	uint64_t _switchTimer = 0;
};

} // namespace stappler::event

#endif /* CORE_EVENT_SPEVENTTHREADHANDLE_H_ */
