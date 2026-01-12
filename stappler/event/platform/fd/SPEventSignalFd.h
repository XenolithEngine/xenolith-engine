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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTSIGNALFD_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTSIGNALFD_H_

#include "SPEventFd.h"

#if __SPRT_CONFIG_HAVE_SIGNALFD

namespace STAPPLER_VERSIONIZED stappler::event {

struct SP_PUBLIC SignalFdSource {
	int fd;
	epoll_event event;

	bool init(const __SPRT_ID(sigset_t) * sig);
	void cancel();
};

class SP_PUBLIC SignalFdHandle : public Handle {
public:
	virtual ~SignalFdHandle() = default;

	bool init(HandleClass *, SpanView<int> sigs);

	bool read();
	bool process();
	void enable();
	void enable(const __SPRT_ID(sigset_t) *);
	void disable();

	const signalfd_siginfo *getInfo() const { return &_info; }

	const __SPRT_ID(sigset_t) * getDefaultSigset() const;
	const __SPRT_ID(sigset_t) * getCurrentSigset() const;

protected:
	__SPRT_ID(sigset_t) _sigset;
	__SPRT_ID(sigset_t) _default;
	signalfd_siginfo _info;
};

class SP_PUBLIC SignalFdURingHandle : public SignalFdHandle {
public:
	virtual ~SignalFdURingHandle() = default;

	Status rearm(URingData *, SignalFdSource *);
	Status disarm(URingData *, SignalFdSource *);

	void notify(URingData *, SignalFdSource *, const NotifyData &);
};

class SP_PUBLIC SignalFdEPollHandle : public SignalFdHandle {
public:
	virtual ~SignalFdEPollHandle() = default;

	Status rearm(EPollData *, SignalFdSource *);
	Status disarm(EPollData *, SignalFdSource *);

	void notify(EPollData *, SignalFdSource *, const NotifyData &);
};

class SP_PUBLIC SignalFdALooperHandle : public SignalFdHandle {
public:
	virtual ~SignalFdALooperHandle() = default;

	Status rearm(ALooperData *, SignalFdSource *);
	Status disarm(ALooperData *, SignalFdSource *);

	void notify(ALooperData *, SignalFdSource *, const NotifyData &);
};

} // namespace stappler::event

#endif

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTSIGNALFD_H_ */
