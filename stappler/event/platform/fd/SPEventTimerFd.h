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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTTIMER_FD_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTTIMER_FD_H_

#include "SPEventFd.h"

namespace STAPPLER_VERSIONIZED stappler::event {

struct SP_PUBLIC TimerFdSource {
	int fd = -1;
	epoll_event event;
	uint64_t target = 0;
	uint32_t count = 0;
	uint32_t value = 0;

	bool init(const TimerInfo &info);
	void cancel();
};

class SP_PUBLIC TimerFdHandle : public TimerHandle {
public:
	virtual ~TimerFdHandle() = default;

	bool init(HandleClass *, TimerInfo &&);

	virtual bool reset(TimerInfo &&) override;

	Status read(uint64_t *);
};

#ifdef SP_EVENT_URING
class SP_PUBLIC TimerFdURingHandle : public TimerFdHandle {
public:
	virtual ~TimerFdURingHandle() = default;

	Status rearm(URingData *, TimerFdSource *);
	Status disarm(URingData *, TimerFdSource *);

	void notify(URingData *, TimerFdSource *source, const NotifyData &);
};
#endif

class SP_PUBLIC TimerFdEPollHandle : public TimerFdHandle {
public:
	virtual ~TimerFdEPollHandle() = default;

	Status rearm(EPollData *, TimerFdSource *);
	Status disarm(EPollData *, TimerFdSource *);

	void notify(EPollData *, TimerFdSource *source, const NotifyData &);
};

#if ANDROID
class SP_PUBLIC TimerFdALooperHandle : public TimerFdHandle {
public:
	virtual ~TimerFdALooperHandle() = default;

	Status rearm(ALooperData *, TimerFdSource *);
	Status disarm(ALooperData *, TimerFdSource *);

	void notify(ALooperData *, TimerFdSource *source, const NotifyData &);
};
#endif

} // namespace stappler::event

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTTIMER_FD_H_ */
