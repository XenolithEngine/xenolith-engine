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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTPOLLFD_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTPOLLFD_H_

#include "SPEventFd.h"
#include "SPEventPollHandle.h"
#include "detail/SPEventHandleClass.h"

namespace STAPPLER_VERSIONIZED stappler::event {

struct PollFdSource {
	int fd;
	epoll_event event;
	PollFlags flags;

	bool init(int, PollFlags);
	void cancel();
};

class SP_PUBLIC PollFdHandle : public PollHandle {
public:
	virtual ~PollFdHandle() = default;

	bool init(HandleClass *, int, PollFlags, CompletionHandle<PollFdHandle> &&);

	virtual bool reset(PollFlags) override;

	virtual NativeHandle getNativeHandle() const override;
};

class SP_PUBLIC PollFdURingHandle : public PollFdHandle {
public:
	virtual ~PollFdURingHandle() = default;

	Status rearm(URingData *, PollFdSource *);
	Status disarm(URingData *, PollFdSource *);

	void notify(URingData *, PollFdSource *, const NotifyData &);
};

class SP_PUBLIC PollFdEPollHandle : public PollFdHandle {
public:
	virtual ~PollFdEPollHandle() = default;

	Status rearm(EPollData *, PollFdSource *);
	Status disarm(EPollData *, PollFdSource *);

	void notify(EPollData *, PollFdSource *, const NotifyData &);
};

class SP_PUBLIC PollFdALooperHandle : public PollFdHandle {
public:
	virtual ~PollFdALooperHandle() = default;

	Status rearm(ALooperData *, PollFdSource *);
	Status disarm(ALooperData *, PollFdSource *);

	void notify(ALooperData *, PollFdSource *, const NotifyData &);
};

} // namespace stappler::event

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTPOLLFD_H_ */
