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

#ifndef CORE_EVENT_PLATFORM_URING_SPEVENTTIMER_URING_H_
#define CORE_EVENT_PLATFORM_URING_SPEVENTTIMER_URING_H_

#include "SPEventThreadHandle.h"
#include "../fd/SPEventEventFd.h"
#include "SPEvent-uring.h"

namespace STAPPLER_VERSIONIZED stappler::event {

/* io_uring timer, based on IORING_TIMEOUT_MULTISHOT
 *
 * It's more performant, then timerfd, but can be stopped from repeating
 * by kernel on unknown reason, so, it should be used only for single-shot
 * or infinite timers
 */

struct TimerUringSource {
	_linux_itimerspec timer;
	uint32_t count = 0;
	uint32_t value = 0;

	bool init(const TimerInfo &info);
	void cancel() { }
};

class SP_PUBLIC TimerURingHandle : public TimerHandle {
public:
	virtual ~TimerURingHandle() = default;

	bool init(HandleClass *, TimerInfo &&);

	virtual bool reset(TimerInfo &&) override;

	Status rearm(URingData *, TimerUringSource *);
	Status disarm(URingData *, TimerUringSource *);

	void notify(URingData *, TimerUringSource *, const NotifyData &);
};

} // namespace stappler::event

#endif /* CORE_EVENT_PLATFORM_URING_SPEVENTTIMER_URING_H_ */
