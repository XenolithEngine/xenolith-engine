/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

#ifndef TESTS_COMPUTE_SRC_COMPUTEHARNESS_H_
#define TESTS_COMPUTE_SRC_COMPUTEHARNESS_H_

#include "SPCommon.h"
#include "XLCoreInstance.h"
#include "XLCoreQueue.h"
#include "XLVkDevice.h"
#include "XLVkLoop.h"
#include "XLVkObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::compute {

// One record of the batch: std430 `vec4`, 16 bytes, no padding.
struct Record {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;
};

enum class FencePath {
	Export, // sync_fd on the looper, where the device and the platform can
	Polled, // the loop's timer
};

enum class SetupResult {
	Ok,
	Skip, // no Vulkan loader, or no physical device
	Fail, // a device exists and the loop or the queue did not come up
};

// The batch the shader transforms. Inputs are small integers, so the transform is exact in float32.
Record makeInput(uint32_t i);
Record expectOutput(const Record &);

// Index of the first record that differs, or `count` when all match. A view of the wrong size is a
// mismatch at 0.
uint32_t verifyOutput(BytesView, uint32_t count);

// What one asynchronous callback delivered. Held by reference from the callback, so a callback that
// comes late - or twice - is still counted.
struct CallState : public Ref {
	uint32_t calls = 0;
	bool success = false;
	Bytes data;
	uint64_t time = 0; // monotonic, microseconds, of the first call
};

// The whole engine side of the test: an instance shared by every rig, one loop with its own device,
// and one compiled compute queue.
class Rig {
public:
	struct Batch {
		Rc<vk::Buffer> target;
		Rc<vk::Buffer> staging;
		uint32_t count = 0;
	};

	static SetupResult prepareInstance(StringView &reason);
	static void releaseInstance();

	SetupResult init(FencePath = FencePath::Export);
	void finalize(uint32_t millis = 5'000);

	vk::Loop *getLoop() const { return _loop; }
	vk::Device *getDevice() const { return _loop ? _loop->getDevice() : nullptr; }

	StringView getDeviceName() const;
	// The device can export fences and this platform listens to them.
	bool canExportFences() const;

	// Staging buffer holding the input, and the device-local target the frame copies it into.
	bool upload(Batch &, uint32_t count);

	Rc<CallState> runFrame(const Batch &);
	Rc<CallState> capture(const Batch &);

	// A fresh queue compiled on this loop; the state says whether it compiled.
	Rc<CallState> compileAnother();

	// Polls the looper until `ready` holds or the time is out.
	bool pump(const Callback<bool()> &ready, uint32_t millis);
	void pumpFor(uint32_t millis);

	// How long an idle pump sleeps, in microseconds. Every hop between the worker and the loop
	// thread waits up to this long, so it sets the floor of any round-trip time.
	static void setPumpQuantum(uint32_t micros);

protected:
	Rc<core::Queue> makeQueue();

	Rc<vk::Loop> _loop;
	Rc<core::Queue> _queue;
	const core::AttachmentData *_records = nullptr;
};

// Kills the process when a section outlives its deadline. The loop thread is the one that hangs, so
// the check cannot live on it.
void watchdogStart();
void watchdogArm(StringView section, uint32_t seconds);
void watchdogDisarm();

// Output is a pipe under the runner: flushed per line, so a killed run still shows what passed.
void flushOutput();

} // namespace stappler::xenolith::compute

#endif /* TESTS_COMPUTE_SRC_COMPUTEHARNESS_H_ */
