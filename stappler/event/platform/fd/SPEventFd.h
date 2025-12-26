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

#ifndef CORE_EVENT_SPEVENT_FD_H_
#define CORE_EVENT_SPEVENT_FD_H_

#include "SPEventHandle.h"
#include "SPEventFileHandle.h"
#include "SPEventTimerHandle.h"

#include "detail/SPEventQueueData.h"

#if SP_POSIX_FD

#include <sys/epoll.h>
#include <sys/signalfd.h>

#if LINUX
#define SP_EVENT_URING
#endif

namespace STAPPLER_VERSIONIZED stappler::event {

struct EPollData;

#if ANDROID
struct ALooperData;
#endif

#ifdef SP_EVENT_URING
struct URingData;

// Extra data can be passed to uring with Handle pointer
// Handle is 32-byte alignment, so, we have 5 bits empty

static constexpr uint64_t URING_USERDATA_USER_MASK = 0b1'1111;
static constexpr uint64_t URING_USERDATA_SERIAL_MASK = 0b0'0111;
static constexpr uint64_t URING_USERDATA_RETAIN_BIT = 0b0'1000;
static constexpr uint64_t URING_USERDATA_ALT_BIT = 0b1'0000;
static constexpr uint64_t URING_USERDATA_PTR_MASK = ~URING_USERDATA_USER_MASK;

// Special userdata values
// DO NOT set RETAIN bit for special values
static constexpr uint64_t URING_USERDATA_IGNORED = maxOf<uint64_t>() & URING_USERDATA_PTR_MASK;
static constexpr uint64_t URING_USERDATA_SUSPENDED =
		maxOf<uint64_t>() & (URING_USERDATA_PTR_MASK | 1);
static constexpr uint64_t URING_USERDATA_TIMEOUT =
		maxOf<uint64_t>() & (URING_USERDATA_PTR_MASK | 2);

#endif

/*class SP_PUBLIC FdSource {
public:
	using URingCallback = void (*) (Handle *, int32_t res, uint32_t flags, URingUserFlags);

	bool init(int fd);

	void cancel();

	int getFd() const { return _fd; }
	void setFd(int fd) { _fd = fd; }

	void setCloseFd(bool val) { _closeFd = val; }

	uint32_t getEventMask() const { return _epoll.event.events; }
	const epoll_event *getEvent() const { return &_epoll.event; }

	URingData *getURingData() const { return _uring.uring; }
	const URingCallback &getCallback() const { return _uring.ucb; }

	void setEpollMask(uint32_t);
	void setURingCallback(URingData *, URingCallback);

	void setTimeoutInterval(TimeInterval, TimeInterval);

	const __kernel_timespec &getTimeout() const {
		return _timer.it_value;
	}

	const __kernel_timespec &getInterval() const {
		return _timer.it_interval;
	}

protected:
	int _fd = -1;
	bool _closeFd = true;
	union {
		struct {
			epoll_event event;
		} _epoll;
		struct {
			URingData *uring;
			URingCallback ucb;
		} _uring;
	};
	union {
		__kernel_itimerspec _timer;
	};
};

class SP_PUBLIC StatURingHandle : public StatHandle {
public:
	virtual ~StatURingHandle() = default;

	bool init(URingData *, StatOpInfo &&);

	Status run(FdSource *);

	void notify(FdSource *, int32_t res, uint32_t flags, URingUserFlags uflags);

protected:
	struct statx _buffer;
};*/

template <typename TimeSpec>
inline void setNanoTimespec(TimeSpec &ts, TimeInterval ival) {
	ts.tv_sec = ival.toSeconds();
	ts.tv_nsec = (ival.toMicros() - ts.tv_sec * 1'000'000ULL) * 1'000;
}

} // namespace stappler::event

#endif // SP_POSIX_FD

#endif /* CORE_EVENT_SPEVENT_FD_H_ */
