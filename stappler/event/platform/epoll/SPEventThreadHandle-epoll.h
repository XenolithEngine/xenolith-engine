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

#ifndef CORE_EVENT_PLATFORM_EPOLL_SPEVENTTHREADHANDLE_EPOLL_H_
#define CORE_EVENT_PLATFORM_EPOLL_SPEVENTTHREADHANDLE_EPOLL_H_

#include "SPEventThreadHandle.h"
#include "../fd/SPEventEventFd.h"
#include "SPEvent-epoll.h"

namespace STAPPLER_VERSIONIZED stappler::event {

// eventfd - based handler
class SP_PUBLIC ThreadEPollHandle : public ThreadHandle {
public:
	virtual ~ThreadEPollHandle() = default;

	bool init(HandleClass *);

	Status read();
	Status write(uint64_t = 1);

	Status rearm(EPollData *, EventFdSource *);
	Status disarm(EPollData *, EventFdSource *);

	void notify(EPollData *, EventFdSource *, const NotifyData &);

	virtual Status perform(Rc<thread::Task> &&task) override;
	virtual Status perform(mem_std::Function<void()> &&func, Ref *target, StringView tag) override;

protected:
	std::mutex _mutex;
};

}

#endif /* CORE_EVENT_PLATFORM_EPOLL_SPEVENTTHREADHANDLE_EPOLL_H_ */
