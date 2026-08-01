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

#ifndef XENOLITH_CORE_XLCOREFRAMEREQUEST_H_
#define XENOLITH_CORE_XLCOREFRAMEREQUEST_H_

#include "XLCorePresentationEngine.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

struct SP_PUBLIC FrameOutputBinding : public Ref {
	using CompleteCallback = Function<bool(FrameAttachmentData &data, bool success, Ref *)>;

	const AttachmentData *attachment = nullptr;
	CompleteCallback callback;
	Rc<Ref> handle;

	FrameOutputBinding(const AttachmentData *, CompleteCallback &&, Rc<Ref> && = nullptr);

	virtual ~FrameOutputBinding();

	bool handleReady(FrameAttachmentData &data, bool success);
};

class SP_PUBLIC FrameRequest final : public Ref {
public:
	using CompleteCallback = FrameOutputBinding::CompleteCallback;

	virtual ~FrameRequest();

	bool init(const Rc<PresentationFrame> &, const Rc<Queue> &q, const FrameConstraints &);
	bool init(const Rc<PresentationFrame> &, const FrameConstraints &);
	bool init(const Rc<Queue> &q);
	bool init(const Rc<Queue> &q, const FrameConstraints &);

	void addSignalDependency(Rc<DependencyEvent> &&);
	void addSignalDependencies(Vector<Rc<DependencyEvent>> &&);

	void addImageSpecialization(const ImageAttachment *, ImageInfoData &&);
	const ImageInfoData *getImageSpecialization(const ImageAttachment *image) const;

	bool addInput(const Attachment *a, Rc<AttachmentInputData> &&);
	bool addInput(const AttachmentData *, Rc<AttachmentInputData> &&);

	void setQueue(const Rc<Queue> &q);

	void setOutput(Rc<FrameOutputBinding> &&);
	void setOutput(const AttachmentData *, CompleteCallback &&, Rc<Ref> && = nullptr);
	void setOutput(const Attachment *a, CompleteCallback &&cb, Rc<Ref> && = nullptr);

	void setRenderTarget(const AttachmentData *, Rc<ImageStorage> &&);

	void attachFrame(FrameHandle *);
	void detachFrame();

	bool onOutputReady(Loop &, FrameAttachmentData &);
	void onOutputInvalidated(Loop &, FrameAttachmentData &);

	void finalize(Loop &, HashMap<const AttachmentData *, FrameAttachmentData *> &attachments,
			bool success);
	void signalDependencies(Loop &, Queue *, bool success);

	Rc<AttachmentInputData> getInputData(const AttachmentData *attachment);

	const Rc<sprt::PoolRef> &getPool() const { return _pool; }

	Rc<ImageStorage> getRenderTarget(const AttachmentData *);

	PresentationFrame *getPresentationFrame() const {
		return _presentationFrame ? _presentationFrame.get() : nullptr;
	}

	const Rc<Queue> &getQueue() const { return _queue; }

	Set<Rc<Queue>> getQueueList() const;

	const FrameConstraints &getFrameConstraints() const { return _constraints; }

	bool isPersistentMapping() const { return _persistentMappings; }

	void setSceneId(uint64_t val) { _sceneId = val; }
	uint64_t getSceneId() const { return _sceneId; }

	// Damage snapshot for this frame. Written once by the worker that builds the renderer's
	// vertex data, before the attachment signals readiness; read afterwards on the loop thread.
	void setDamageState(Rc<FrameDamageState> &&val) { _damage = sp::move(val); }
	const Rc<FrameDamageState> &getDamageState() const { return _damage; }

	// Set when a pass found the target image already holding this exact frame and recorded nothing.
	// Informational for present (which still runs, against its own baseline) and for damage logging.
	void setRedrawSkipped(bool val) { _redrawSkipped = val; }
	bool isRedrawSkipped() const { return _redrawSkipped; }

	// Absolute monotonic-clock deadline (microseconds) by which this frame must complete; 0 = none.
	// The PresentationEngine cancels the frame if it is still pending at the deadline (e.g. an input
	// or dependency that never arrives). It seeds a default if the caller leaves it unset.
	void setDeadline(uint64_t val) { _deadline = val; }
	uint64_t getDeadline() const { return _deadline; }

	const Vector<Rc<DependencyEvent>> &getSignalDependencies() const { return _signalDependencies; }

	FrameRequest() = default;

	void waitForInput(FrameQueue &, AttachmentHandle &a, Function<void(bool)> &&cb);

	const FrameOutputBinding *getOutputBinding(const AttachmentData *) const;

	void autorelease(Ref *ref) { _autorelease.emplace_front(ref); }

protected:
	FrameRequest(const FrameRequest &) = delete;
	FrameRequest &operator=(const FrameRequest &) = delete;

	Rc<sprt::PoolRef> _pool;
	Rc<PresentationFrame> _presentationFrame;
	Rc<Queue> _queue;
	FrameConstraints _constraints;
	Map<const AttachmentData *, Rc<AttachmentInputData>> _input;

	// if true, do not wait synchronization with other active frames in emitter
	bool _readyForSubmit = true;

	// try to map per-frame GPU memory persistently
	bool _persistentMappings = true;
	uint64_t _sceneId = 0;
	Rc<FrameDamageState> _damage;
	bool _redrawSkipped = false;
	uint64_t _deadline = 0; // absolute monotonic-clock deadline (us); 0 = none

	Map<const ImageAttachment *, ImageInfoData> _imageSpecialization;
	Map<const AttachmentData *, Rc<FrameOutputBinding>> _output;
	Map<const AttachmentData *, Rc<ImageStorage>> _renderTargets;

	Vector<Rc<DependencyEvent>> _signalDependencies;

	struct WaitInputData {
		Rc<FrameQueue> queue;
		AttachmentHandle *handle;
		Function<void(bool)> callback;
	};

	Map<const AttachmentData *, WaitInputData> _waitForInputs;
	FrameHandle *_frame = nullptr;
	sprt::__malloc_forward_list<Rc<Ref>> _autorelease;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREFRAMEREQUEST_H_ */
