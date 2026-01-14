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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_INTERFACE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_INTERFACE_H_

#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/surface_info.h>
#include <sprt/runtime/window/presentation.h>
#include <sprt/runtime/window/types.h>
#include <sprt/runtime/filesystem/lookup.h>

namespace sprt::window {

class SPRT_API AppWindow : public Ref {
public:
	virtual ~AppWindow() = default;

	virtual void run() = 0;
	virtual void close(bool force) = 0;
	virtual void setReadyForNextFrame() = 0;
	virtual void update(PresentationUpdateFlags) = 0;

	virtual bool waitUntilFrame() = 0;
};

class SPRT_API HandleAdapter : public Ref {
public:
	virtual ~HandleAdapter() = default;

	virtual Status getStatus() const = 0;

	virtual Status resume() = 0;
	virtual Status pause() = 0;

	virtual Status cancel(Status = Status::ErrorCancelled, uint32_t value = 0) = 0;
	virtual void setUserdata(Ref *) = 0;
	virtual Ref *getUserdata() const = 0;

	virtual bool resetPoll(filesystem::PollFlags) = 0;
	virtual bool resetTimer(time_t timeout, time_t interval, uint32_t count) = 0;
};

class SPRT_API LooperAdapter : public Ref {
public:
	static constexpr uint32_t TimerInfinite = Max<uint32_t>;

	static LooperAdapter *getForThread();
	static void setForThread(LooperAdapter *);

	virtual ~LooperAdapter() = default;

	virtual void poll() = 0;
	virtual Status wakeup(bool graceful = false) = 0;
	virtual Status run() = 0;

	virtual int getHandle() const = 0;

	virtual Rc<HandleAdapter> scheduleTimer(time_t timeout, time_t interval, uint32_t count, void *,
			void (*)(void *, HandleAdapter *, uint32_t flags, Status status)) = 0;

	virtual Rc<HandleAdapter> listenPollableHandle(int, filesystem::PollFlags, void *,
			void (*)(void *, HandleAdapter *, uint32_t flags, Status status)) = 0;

	virtual Rc<HandleAdapter> listenPollableHandle(int, filesystem::PollFlags,
			Function<Status(int fd, filesystem::PollFlags flags)> &&, Ref * = nullptr) = 0;

	virtual Status performOnThread(memory::dynfunction<void()> &&func, Ref *target = nullptr,
			bool immediate = false, StringView tag = __SPRT_LOCATION.function_name()) const = 0;

	virtual Status performAsync(memory::dynfunction<void()> &&, Ref * = nullptr, bool first = false,
			StringView tag = __SPRT_LOCATION.function_name()) const = 0;
};

} // namespace sprt::window

#endif
