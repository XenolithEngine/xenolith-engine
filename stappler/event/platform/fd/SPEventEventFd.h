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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTEVENTFD_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTEVENTFD_H_

#include "SPEventFd.h"

namespace STAPPLER_VERSIONIZED stappler::event {

struct SP_PUBLIC EventFdSource {
	static constexpr int TARGET_BUFFER_COUNT = Handle::DataSize / sizeof(uint64_t) - 1;

	uint32_t eventValue; // use atomic accessors
	int32_t fd = -1;
	union {
		struct {
			epoll_event event;
			uint64_t eventTarget;
		};
		uint64_t target[TARGET_BUFFER_COUNT] = {0};
	};

	bool init();
	void cancel();
};

class SP_PUBLIC EventFdHandle : public Handle {
public:
	virtual ~EventFdHandle() = default;

	bool init(HandleClass *, CompletionHandle<void> &&);

	Status read(uint64_t *target = nullptr);
	Status write(uint64_t = 1, uint32_t value = 0);
};

#ifdef SP_EVENT_URING
class SP_PUBLIC EventFdURingHandle : public EventFdHandle {
public:
	virtual ~EventFdURingHandle() = default;

	Status rearm(URingData *, EventFdSource *);
	Status disarm(URingData *, EventFdSource *);

	void notify(URingData *, EventFdSource *, const NotifyData &);
};
#endif

class SP_PUBLIC EventFdEPollHandle : public EventFdHandle {
public:
	virtual ~EventFdEPollHandle() = default;

	Status rearm(EPollData *, EventFdSource *);
	Status disarm(EPollData *, EventFdSource *);

	void notify(EPollData *, EventFdSource *, const NotifyData &);
};

#if ANDROID
class SP_PUBLIC EventFdALooperHandle : public EventFdHandle {
public:
	virtual ~EventFdALooperHandle() = default;

	Status rearm(ALooperData *, EventFdSource *);
	Status disarm(ALooperData *, EventFdSource *);

	void notify(ALooperData *, EventFdSource *, const NotifyData &);
};
#endif

} // namespace stappler::event

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTEVENTFD_H_ */
