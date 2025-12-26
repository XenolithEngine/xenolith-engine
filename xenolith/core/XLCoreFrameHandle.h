/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_CORE_XLCOREFRAMEHANDLE_H_
#define XENOLITH_CORE_XLCOREFRAMEHANDLE_H_

#include "XLCoreQueue.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class PresentationEngine;

// handle for a running external task
// when it's destroyed - task will be marked as completed
class SP_PUBLIC FrameExternalTask final : public Ref {
public:
	virtual ~FrameExternalTask();

	// mark task as failed, so, it's frame will be invalidated too
	void invalidate();

	uint32_t getIndex() const { return _index; }
	StringView getTag() const { return _tag; }
	Ref *getRef() const { return _userdata; }

protected:
	friend class FrameHandle;

	bool init(FrameHandle &, uint32_t idx, Ref *, StringView tag);

	bool _success = true;
	uint32_t _index = 0;
	StringView _tag;
	Rc<FrameHandle> _frame;
	Rc<Ref> _userdata;
};

class SP_PUBLIC FrameHandle : public Ref {
public:
	using FrameRequest = core::FrameRequest;

	static uint32_t GetActiveFramesCount();
	static void DescribeActiveFrames();

	virtual ~FrameHandle();

	bool init(Loop &, Device &, Rc<FrameRequest> &&, uint64_t gen);

	void update(bool init = false);

	uint64_t getTimeStart() const { return _timeStart; }
	uint64_t getTimeEnd() const { return _timeEnd; }
	uint64_t getOrder() const { return _order; }
	uint64_t getGen() const { return _gen; }
	uint64_t getSubmissionTime() const { return _submissionTime; }
	uint64_t getDeviceTime() const { return _deviceTime; }
	Loop *getLoop() const { return _loop; }
	Device *getDevice() const { return _device; }

	const Rc<Queue> &getQueue() const;
	const FrameConstraints &getFrameConstraints() const;
	const Rc<PoolRef> &getPool() const { return _pool; }
	const Rc<FrameRequest> &getRequest() const { return _request; }

	const ImageInfoData *getImageSpecialization(const ImageAttachment *) const;

	const FrameOutputBinding *getOutputBinding(const Attachment *a) const;
	const FrameOutputBinding *getOutputBinding(const AttachmentData *a) const;
	Rc<ImageStorage> getRenderTarget(const Attachment *a) const;
	Rc<ImageStorage> getRenderTarget(const AttachmentData *a) const;
	const Vector<Rc<DependencyEvent>> &getSignalDependencies() const;

	const Vector<Rc<FrameQueue>> &getFrameQueues() const { return _queues; }
	FrameQueue *getFrameQueue(Queue *) const;

	Rc<FrameExternalTask> acquireTask(Ref * = nullptr, StringView tag = StringView());

	// thread tasks within frame should not be performed directly on loop's queue to preserve FrameHandle object
	virtual void performInQueue(Function<void(FrameHandle &)> &&, Ref *, StringView tag);
	virtual void performInQueue(Function<bool(FrameHandle &)> &&,
			Function<void(FrameHandle &, bool)> &&, Ref *, StringView tag);

	// thread tasks within frame should not be performed directly on loop's queue to preserve FrameHandle object
	virtual void performOnGlThread(Function<void(FrameHandle &)> &&, Ref *, bool immediate,
			StringView tag);

	// required tasks should be completed before onComplete call
	virtual void performRequiredTask(Function<bool(FrameHandle &)> &&, Ref *, StringView tag);
	virtual void performRequiredTask(Function<bool(FrameHandle &)> &&,
			Function<void(FrameHandle &, bool)> &&, Ref *, StringView tag);

	virtual bool isSubmitted() const { return _submitted; }
	virtual bool isValid() const;
	virtual bool isValidFlag() const { return _valid; }

	virtual bool isPersistentMapping() const;

	virtual Rc<AttachmentInputData> getInputData(const AttachmentData *);

	virtual void invalidate();

	virtual void setCompleteCallback(Function<void(FrameHandle &)> &&);

	virtual void onQueueSubmitted(FrameQueue &);
	virtual void onQueueComplete(FrameQueue &);
	virtual void onQueueInvalidated(FrameQueue &);
	virtual bool onOutputAttachment(FrameAttachmentData &);
	virtual void onOutputAttachmentInvalidated(FrameAttachmentData &);

	virtual void waitForDependencies(const Vector<Rc<DependencyEvent>> &,
			Function<void(FrameHandle &, bool)> &&);

	virtual void waitForInput(FrameQueue &queue, AttachmentHandle &a, Function<void(bool)> &&cb);

	virtual void signalDependencies(bool success);

protected:
	friend class FrameExternalTask;

	virtual bool setup();

	void releaseTask(FrameExternalTask *, bool success);

	virtual void tryComplete();
	virtual void onComplete();

	// Uncomment to track retain/release cycles
	//#if SP_REF_DEBUG
	//	virtual bool isRetainTrackerEnabled() const override { return true; }
	//#endif

	Loop *_loop = nullptr; // loop can not die until frames are performed
	Rc<Device> _device;
	Rc<PoolRef> _pool;
	Rc<FrameRequest> _request;

	uint64_t _timeStart = 0;
	uint64_t _timeEnd = 0;
	uint64_t _gen = 0;
	uint64_t _order = 0;
	uint64_t _submissionTime = 0;
	uint64_t _deviceTime = 0;
	std::atomic<uint32_t> _tasksRequired = 0;
	uint32_t _tasksCompleted = 0;
	uint32_t _queuesSubmitted = 0;
	uint32_t _queuesCompleted = 0;

	bool _submitted = false;
	bool _completed = false;
	bool _valid = true;
	bool _persistentMappings = false;

	Vector<Rc<FrameQueue>> _queues;
	Function<void(FrameHandle &)> _complete;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREFRAMEHANDLE_H_ */
