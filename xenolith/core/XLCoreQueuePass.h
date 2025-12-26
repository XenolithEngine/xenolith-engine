/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_CORE_XLCOREQUEUEPASS_H_
#define XENOLITH_CORE_XLCOREQUEUEPASS_H_

#include "XLCoreObject.h"
#include "XLCoreFrameQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class SP_PUBLIC QueuePass : public NamedRef {
public:
	using Queue = core::Queue;
	using FrameQueue = core::FrameQueue;
	using RenderOrdering = core::RenderOrdering;
	using QueuePassBuilder = core::QueuePassBuilder;
	using QueuePassHandle = core::QueuePassHandle;
	using PassType = core::PassType;
	using AttachmentData = core::AttachmentData;

	using FrameHandleCallback = Function<Rc<QueuePassHandle>(QueuePass &, const FrameQueue &)>;

	virtual ~QueuePass();

	virtual bool init(QueuePassBuilder &);
	virtual void invalidate();

	virtual StringView getName() const override;
	virtual RenderOrdering getOrdering() const;
	virtual size_t getSubpassCount() const;
	virtual PassType getType() const;

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &);

	void setFrameHandleCallback(FrameHandleCallback &&);

	const QueuePassData *getData() const { return _data; }

protected:
	friend class core::Queue;

	// called before compilation
	virtual void prepare(Device &);

	Function<Extent2(const FrameQueue &)> _frameSizeCallback;
	FrameHandleCallback _frameHandleCallback;
	const QueuePassData *_data = nullptr;
};

class SP_PUBLIC QueuePassHandle : public NamedRef {
public:
	using QueuePass = core::QueuePass;
	using FrameHandle = core::FrameHandle;
	using FrameQueue = core::FrameQueue;
	using FrameSync = core::FrameSync;
	using RenderOrdering = core::RenderOrdering;

	virtual ~QueuePassHandle();

	virtual bool init(QueuePass &, const FrameQueue &);
	virtual void setQueueData(FramePassData &);
	virtual const FramePassData *getQueueData() const;

	virtual StringView getName() const override;

	virtual const QueuePassData *getData() const { return _data; }
	virtual QueuePass *getQueuePass() const { return _queuePass; }
	virtual Framebuffer *getFramebuffer() const;

	virtual Fence *getFence() const { return _fence; }

	// Check if render pass can be performed for frame
	// `isAvailable` should be called before `prepare`, but after all pass resources
	// are acquired
	// If true - pass will be processed as usual
	// If false - pass will be skipped and immediately set to Complete state
	virtual bool isAvailable(const FrameQueue &) const;

	virtual bool isSubmitted() const;
	virtual bool isCompleted() const;

	virtual bool isFramebufferRequired() const;

	// Run data preparation process, that do not require queuing
	// returns true if 'prepare' completes immediately (either successful or not)
	// returns false if 'prepare' run some subroutines, and we should wait for them
	// To indicate success, call callback with 'true'. For failure - with 'false'
	// To indicate immediate failure, call callback with 'false', then return true;
	virtual bool prepare(FrameQueue &, Function<void(bool)> &&);

	// Run queue submission process
	// If submission were successful, you should call onSubmited callback with 'true'
	// If submission failed, call onSubmited with 'false'
	// If submission were successful, onComplete should be called when execution completes
	virtual void submit(FrameQueue &, Rc<FrameSync> &&, Function<void(bool)> &&onSubmited,
			Function<void(bool)> &&onComplete);

	// after submit
	virtual void finalize(FrameQueue &, bool successful);

	virtual AttachmentHandle *getAttachmentHandle(const AttachmentData *) const;

	void autorelease(Ref *) const;

	const AttachmentPassData *getAttachemntData(const AttachmentData *) const;

protected:
	virtual void prepareSubpasses(FrameQueue &);

	Rc<QueuePass> _queuePass;
	const QueuePassData *_data = nullptr;
	FramePassData *_queueData = nullptr;

	Loop *_loop = nullptr;
	Rc<Fence> _fence;

	mutable Mutex _autoreleaseMutex;
	mutable Vector<Rc<Ref>> _autorelease;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREQUEUEPASS_H_ */
