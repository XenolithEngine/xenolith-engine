/**
Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_BACKEND_VK_XLVKSYNC_H_
#define XENOLITH_BACKEND_VK_XLVKSYNC_H_

#include "XLVk.h"
#include "XLCoreObject.h"

#include <sprt/runtime/dispatch/handle.h>
#include <sprt/cxx/atomic>

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class Device;
class DeviceQueue;
class Loop;

/* VkSemaphore wrapper
 *
 * usage pattern:
 * - store handles in common storage
 * - pop one before running signal function
 * - run function, that signals VkSemaphore, handle should be acquired with getSemaphore()
 * - run function, that waits on VkSemaphore
 * - push Semaphore back into storage
 */

class SP_PUBLIC Semaphore : public core::Semaphore {
public:
	virtual ~Semaphore();

	bool init(Device &, core::SemaphoreType);

	VkSemaphore getSemaphore() const { return _sem; }

protected:
	using core::Semaphore::init;

	VkSemaphore _sem = VK_NULL_HANDLE;
};

class SP_PUBLIC Fence final : public core::Fence {
public:
	virtual ~Fence() = default;

	bool init(Device &, core::FenceType);

	VkFence getFence() const { return _fence; }

	// Exports the fence as a sync_fd and listens to it on the loop's looper. Null when the device
	// cannot export, export is off, or the fence has already signaled - the caller polls it then.
	// `onReleased` runs on the looper once the handle has released the fence.
	//
	// The export moves the pending signal into the fd: the fence itself may stay unsignaled for good
	// (NVIDIA does). An exported fence is released through `checkExternal` only - `check` without
	// `lockfree` would wait on it forever. Its status is still good for one thing: a lost device.
	Rc<sprt::dispatch::PollHandle> exportFence(Loop &, Function<void()> &&onReleased);

protected:
	using core::Object::init;

	virtual Status doCheckFence(bool lockfree) override;
	virtual void doResetFence() override;

	VkFence _fence = VK_NULL_HANDLE;
	bool _exportable = false;
	sprt::atomic<bool> _externalSignal = false; // the exported sync_fd has fired since the last reset
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKSYNC_H_ */
