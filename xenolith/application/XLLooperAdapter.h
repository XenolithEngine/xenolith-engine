/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef XENOLITH_APPLICATION_XLLOOPERADAPTER_H_
#define XENOLITH_APPLICATION_XLLOOPERADAPTER_H_

#include "XLCommon.h"
#include "SPEventLooper.h"

#include <sprt/runtime/window/interface.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class SPRT_API HandleAdapter : public sprt::window::HandleAdapter {
public:
	using Fn = void (*)(void *, sprt::window::HandleAdapter *, uint32_t flags, Status status);

	enum Type {
		Timer,
		Poll
	};

	virtual ~HandleAdapter() = default;

	virtual bool init(Type, void *, Fn);
	virtual bool init(Type, Rc<event::Handle> &&);

	virtual void setHandle(Rc<event::Handle> &&);
	virtual void forward(uint32_t flags, Status status);

	virtual Status getStatus() const override;

	virtual Status resume() override;
	virtual Status pause() override;

	virtual Status cancel(Status = Status::ErrorCancelled, uint32_t value = 0) override;

	virtual void setUserdata(Ref *) override;
	virtual Ref *getUserdata() const override;

	virtual bool resetPoll(filesystem::PollFlags) override;
	virtual bool resetTimer(time_t timeout, time_t interval, uint32_t count) override;

protected:
	Type _type = Type::Timer;
	Rc<event::Handle> _handle;
	void *_ptr = nullptr;
	Fn _fn = nullptr;
};

class SPRT_API LooperAdapter : public sprt::window::LooperAdapter {
public:
	virtual ~LooperAdapter() = default;

	virtual bool init(NotNull<event::Looper>);

	virtual void poll() override;
	virtual Status wakeup(bool graceful = false) override;
	virtual Status run() override;

	virtual Rc<sprt::window::HandleAdapter> scheduleTimer(time_t timeout, time_t interval,
			uint32_t count, void *,
			void (*)(void *, sprt::window::HandleAdapter *, uint32_t flags,
					Status status)) override;

	virtual Rc<sprt::window::HandleAdapter> listenPollableHandle(int, filesystem::PollFlags, void *,
			void (*)(void *, sprt::window::HandleAdapter *, uint32_t flags,
					Status status)) override;

	virtual Rc<sprt::window::HandleAdapter> listenPollableHandle(int, filesystem::PollFlags,
			sprt::memory::dynfunction<Status(int fd, filesystem::PollFlags flags)> &&,
			Ref * = nullptr) override;

	virtual Status performOnThread(sprt::memory::dynfunction<void()> &&func, Ref *target = nullptr,
			bool immediate = false,
			StringView tag = __SPRT_LOCATION.function_name()) const override;

protected:
	Rc<event::Looper> _looper;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_XLLOOPERADAPTER_H_
