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

#ifndef CORE_EVENT_PLATFORM_WINDOWS_SPEVENTTIMERWIN_H_
#define CORE_EVENT_PLATFORM_WINDOWS_SPEVENTTIMERWIN_H_

#include <sprt/runtime/dispatch/handle.h>
#include "SPEvent-iocp.h"

namespace sprt::dispatch {

class SPRT_API TimerWinHandle;

struct SPRT_API TimerWinSource {
	void *queue = nullptr;
	void *handle = nullptr;
	DWORD timeout;
	DWORD interval;
	uint32_t count = 0;
	uint32_t value = 0;
	bool active = false;

	bool init(const TimerInfo &info);

	bool start(HANDLE queue, TimerWinHandle *iocp);
	void stop(TimerWinHandle *h);
	void reset(TimerWinHandle *h);
	void cancel(Handle *h);
};

class SPRT_API TimerWinHandle : public TimerHandle {
public:
	virtual ~TimerWinHandle() = default;

	bool init(HandleClass *, TimerInfo &&);

	virtual bool reset(TimerInfo &&) override;

	Status rearm(IocpData *, TimerWinSource *);
	Status disarm(IocpData *, TimerWinSource *);

	void notify(IocpData *, TimerWinSource *source, const NotifyData &);
};

} // namespace sprt::dispatch

#endif // CORE_EVENT_PLATFORM_WINDOWS_SPEVENTTIMERWIN_H_
