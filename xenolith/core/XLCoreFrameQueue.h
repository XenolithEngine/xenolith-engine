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

#ifndef XENOLITH_CORE_XLCOREFRAMEQUEUE_H_
#define XENOLITH_CORE_XLCOREFRAMEQUEUE_H_

#include "XLCoreQueueData.h"
#include "XLCoreImageStorage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

struct FramePassData;
struct FrameAttachmentData;

struct SP_PUBLIC FrameSyncAttachment {
	const AttachmentHandle *attachment;
	Rc<Semaphore> semaphore;
	ImageStorage *image = nullptr;
	PipelineStage stages = PipelineStage::None;
};

struct SP_PUBLIC FramePassData {
	FrameRenderPassState state = FrameRenderPassState::Initial;
	Rc<QueuePassHandle> handle;
	const QueuePassData *data = nullptr;

	Vector<Pair<const AttachmentPassData *, FrameAttachmentData *>> attachments;
	HashMap<const AttachmentData *, FrameAttachmentData *> attachmentMap;

	HashMap<FrameRenderPassState, Vector<FramePassData *>> waiters;

	mutable Vector<FrameSyncAttachment> waitSync;

	Rc<Framebuffer> framebuffer;
	bool waitForResult = false;

	uint64_t submitTime = 0;
	uint64_t deviceTime = 0;
};

struct SP_PUBLIC FrameAttachmentData {
	FrameAttachmentState state = FrameAttachmentState::Initial;
	Rc<AttachmentHandle> handle;
	ImageInfoData info;

	Vector<FramePassData *> passes;

	// state of final RenderPass, on which Attachment resources can be released
	FrameRenderPassState final;

	Rc<ImageStorage> image;
	bool waitForResult = false;
};

struct SP_PUBLIC FrameSyncImage {
	const AttachmentHandle *attachment;
	ImageStorage *image = nullptr;
	AttachmentLayout newLayout = AttachmentLayout::Undefined;
};

struct SP_PUBLIC FrameSync : public Ref {
	Vector<FrameSyncAttachment> waitAttachments;
	Vector<FrameSyncAttachment> signalAttachments;
	Vector<FrameSyncImage> images;
};

class SP_PUBLIC FrameQueue final : public Ref {
public:
	virtual ~FrameQueue();

	bool init(const Rc<PoolRef> &, const Rc<Queue> &, FrameHandle &);

	bool setup();
	void update();
	void invalidate();

	bool isFinalized() const { return _finalized; }

	const Rc<FrameHandle> &getFrame() const { return _frame; }
	const Rc<PoolRef> &getPool() const { return _pool; }
	const Rc<Queue> &getQueue() const { return _queue; }
	Loop *getLoop() const;

	const HashMap<const QueuePassData *, FramePassData> &getRenderPasses() const {
		return _renderPasses;
	}
	const HashMap<const AttachmentData *, FrameAttachmentData> &getAttachments() const {
		return _attachments;
	}
	uint64_t getSubmissionTime() const { return _submissionTime; }
	uint64_t getDeviceTime() const { return _deviceTime; }

	const FrameAttachmentData *getAttachment(const AttachmentData *) const;
	const FramePassData *getRenderPass(const QueuePassData *) const;

protected:
	bool isResourcePending(const FrameAttachmentData &);
	void waitForResource(const FrameAttachmentData &, Function<void(bool)> &&);

	bool isResourcePending(const FramePassData &);
	void waitForResource(const FramePassData &, Function<void()> &&);

	void onAttachmentSetupComplete(FrameAttachmentData &);
	void onAttachmentInput(FrameAttachmentData &);
	void onAttachmentAcquire(FrameAttachmentData &);
	void onAttachmentRelease(FrameAttachmentData &,
			FrameAttachmentState state = FrameAttachmentState::ResourcesReleased);

	bool isRenderPassReady(const FramePassData &) const;
	bool isRenderPassReadyForState(const FramePassData &, FrameRenderPassState) const;
	void updateRenderPassState(FramePassData &, FrameRenderPassState);

	void onRenderPassReady(FramePassData &);
	void onRenderPassResourcesAcquired(FramePassData &);
	void onRenderPassPrepared(FramePassData &);
	void onRenderPassSubmission(FramePassData &);
	void onRenderPassSubmitted(FramePassData &);
	void onRenderPassComplete(FramePassData &);

	Rc<FrameSync> makeRenderPassSync(FramePassData &) const;
	PipelineStage getWaitStageForAttachment(FramePassData &data,
			const AttachmentHandle *handle) const;

	void onComplete();
	void onFinalized();

	void invalidate(FrameAttachmentData &);
	void invalidate(FramePassData &);

	void tryReleaseFrame();

	void finalizeAttachment(FrameAttachmentData &);

	Rc<PoolRef> _pool;
	Rc<Queue> _queue;
	Rc<FrameHandle> _frame;
	Loop *_loop = nullptr;
	uint64_t _order = 0;
	bool _finalized = false;
	bool _success = false;
	bool _invalidated = false;

	HashMap<const QueuePassData *, FramePassData> _renderPasses;
	HashMap<const AttachmentData *, FrameAttachmentData> _attachments;

	HashSet<FramePassData *> _renderPassesInitial;
	HashSet<FramePassData *> _renderPassesPrepared;
	HashSet<FrameAttachmentData *> _attachmentsInitial;

	std::forward_list<Rc<Ref>> _autorelease;
	uint32_t _renderPassSubmitted = 0;
	uint32_t _renderPassCompleted = 0;

	uint32_t _finalizedObjects = 0;
	uint64_t _submissionTime = 0;
	uint64_t _deviceTime = 0;

	Vector<Pair<FramePassData *, FrameRenderPassState>> _awaitPasses;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREFRAMEQUEUE_H_ */
