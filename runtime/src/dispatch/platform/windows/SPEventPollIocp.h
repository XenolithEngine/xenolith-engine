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

#ifndef CORE_EVENT_PLATFORM_WINDOWS_SPEVENTPOLLIOCP_H_
#define CORE_EVENT_PLATFORM_WINDOWS_SPEVENTPOLLIOCP_H_

#include "SPEventHandle.h"
#include "SPEventPollHandle.h"

#if WIN32

#include "SPEvent-iocp.h"

namespace sprt::dispatch {

struct PollIocpSource {
	void *handle = nullptr;
	void *event = nullptr;
	PollFlags flags = PollFlags::None;

	bool init(void *, PollFlags);
	void cancel();
};

class SPRT_API PollIocpHandle : public PollHandle {
public:
	/*static Rc<PollHandle> create(const Queue *, HANDLE,
			CompletionHandle<PollHandle> &&);

	static Rc<PollHandle> create(const Queue *, HANDLE,
			mem_std::Function<Status(HANDLE)> &&, Ref *ref);*/

	virtual ~PollIocpHandle() = default;

	bool init(HandleClass *, void *, PollFlags, CompletionHandle<PollHandle> &&);

	virtual NativeHandle getNativeHandle() const override;

	virtual bool reset(PollFlags) override;

	Status rearm(IocpData *, PollIocpSource *);
	Status disarm(IocpData *, PollIocpSource *);

	void notify(IocpData *, PollIocpSource *, const NotifyData &);
};

} // namespace sprt::dispatch

#endif

#endif /* CORE_EVENT_PLATFORM_WINDOWS_SPEVENTPOLLIOCP_H_ */
