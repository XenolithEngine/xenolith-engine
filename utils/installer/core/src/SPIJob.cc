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

#include "SPIJob.h"

#include <sprt/runtime/dispatch/thread.h>
#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace dispatch = sprt::dispatch;

namespace {

class JobThread : public dispatch::Thread {
public:
	virtual ~JobThread() = default;

	bool init(const Callback<void()> *fn) {
		_fn = fn;
		return true;
	}

	Status getStatus() const { return _status; }

	virtual void threadInit() override {
		Thread::threadInit();

		// The mask must stay identical to the one makefile::runBuild uses
		// (stappler/makefile/SPMakefileBuilder.cc) — the first acquire for a thread wins, so this
		// one decides which queue engine a nested runBuild gets.
		auto engine = dispatch::QueueEngine::Any;
		engine &= ~dispatch::QueueEngine::ALooper; // no spawnProcess support
		engine &= ~dispatch::QueueEngine::RunLoop; // spawnProcess is emulated there

		_looper = dispatch::Looper::acquire(dispatch::LooperInfo{
			.name = StringView("installer"),
			.workersCount = 0,
			.engineMask = engine,
		});
	}

	// A single pass: run the job, then let the thread (and its Looper) go.
	virtual bool worker() override {
		if (!_looper) {
			_status = Status::ErrorNotImplemented;
			return false;
		}

		auto pool = memory::pool::create(static_cast<memory::pool_t *>(nullptr));
		memory::perform([&] { (*_fn)(); }, pool);
		memory::pool::destroy(pool);

		_status = Status::Done;
		return false;
	}

protected:
	const Callback<void()> *_fn = nullptr;
	dispatch::Looper *_looper = nullptr;
	Status _status = Status::ErrorCancelled;
};

} // namespace

Status runJob(const Callback<void()> &fn) {
	auto thread = Rc<JobThread>::create(&fn);
	if (!thread) {
		return Status::ErrorNotPermitted;
	}

	if (!thread->run(dispatch::ThreadFlags::Joinable)) {
		return Status::ErrorNotPermitted;
	}

	thread->waitStopped();
	return thread->getStatus();
}

} // namespace stappler::xenolith::installer
