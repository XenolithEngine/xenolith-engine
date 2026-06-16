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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_EVENT_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_EVENT_H_

#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/utils/native_handle.h>
#include <sprt/runtime/dispatch/types.h>

namespace sprt::dispatch {

class Looper;
class Queue;
class Handle;

class TimerHandle;
class ThreadHandle;
class PollHandle;
class ProcessHandle;

struct BufferChain;

using NativeHandle = sprt::native_handle;

using filesystem::OpenFlags;
using filesystem::PollFlags;

// Type-erased completion callback. The reinterpret_casts below cast between
// function-pointer types that differ only in a pointer parameter's pointee
// (T* vs void* userdata, and Result* across CompletionHandle<Other>). Calling
// through a differently-typed function pointer is technically UB, but it is safe
// on every supported ABI (all object pointers share representation and calling
// convention), and it is the intentional erasure mechanism here.
template <typename Result = Handle>
struct SPRT_API CompletionHandle {
	using Fn = void (*)(void *, Result *, uint32_t value, Status);

	template <typename T>
	static CompletionHandle create(T *ptr, void (*cb)(T *, Result *, uint32_t value, Status)) {
		CompletionHandle ret;
		ret.userdata = reinterpret_cast<void *>(ptr);
		ret.fn = reinterpret_cast<Fn>(cb);
		return ret;
	}

	template <typename Other>
	CompletionHandle &operator=(const CompletionHandle<Other> &other) {
		fn = reinterpret_cast<Fn>(other.fn);
		userdata = other.userdata;
		return *this;
	}

	template <typename Other>
	operator CompletionHandle<Other>() const {
		CompletionHandle<Other> ret;
		ret.fn = reinterpret_cast<typename CompletionHandle<Other>::Fn>(fn);
		ret.userdata = userdata;
		return ret;
	}

	operator bool() const { return fn; }

	Fn fn = nullptr;
	void *userdata = nullptr;
};

struct SPRT_API TimerInfo {
	static constexpr uint32_t Infinite = Max<uint32_t>;

	using Completion = CompletionHandle<TimerHandle>;

	Completion completion;
	TimeInterval timeout;
	TimeInterval interval;
	uint32_t count = 0;

	// ClockType for a timer only partially usable on non-Linux systems
	// Just leave it Default
	__sprt_clockid_t clock = __SPRT_CLOCK_REALTIME;

	// Set this = true if you want to use `TimerHandle::reset`.
	//
	// Without this flag, you can still call `TimerHandle::reset`, but it can be
	// only partially available, some variants of TimerInfo can be rejected
	// (`TimerHandle::reset` returns `false`)
	//
	// Note: that resetable timer CAN be less performant then regular as a timer,
	// but 'reset' for this timer can save some syscalls and kernel resources
	bool resetable = false;
};

// Parameters for Looper/Queue::spawnProcess.
//
// The command is launched through the system shell (/bin/sh -c on POSIX,
// cmd.exe /c on Windows); stdout and stderr are merged onto a single pipe.
// `reader` is invoked on the looper thread with each chunk of output as it
// arrives (it may be called many times, or never). The returned ProcessHandle's
// completion fires once when the process exits, with `value` carrying the exit
// code (128 + signal number if the process was killed by a signal).
struct SPRT_API ProcessInfo {
	using ReaderCallback = Function<void(StringView)>;
	using Completion = CompletionHandle<ProcessHandle>;

	StringView command;
	ReaderCallback reader;
	Completion completion;
};

} // namespace sprt::dispatch

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_EVENT_H_ */
