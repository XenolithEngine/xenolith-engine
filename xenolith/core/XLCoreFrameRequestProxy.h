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

#ifndef XENOLITH_CORE_XLCOREFRAMEREQUESTPROXY_H_
#define XENOLITH_CORE_XLCOREFRAMEREQUESTPROXY_H_

#include "XLCoreFrameRequest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

//
// FrameRequestProxy: the client's per-frame command-batch builder, abstracting how the batch
// reaches the server's gapi backend (see XLCoreRenderSession.h):
//   - LocalFrameRequestProxy  (single process): forwards directly to the server's FrameRequest.
//   - RemoteFrameRequestProxy (networked):      accumulates + serializes; the server rebuilds a
//                                               FrameRequest from the bytes. (Stubbed this stage.)
//
// The client surface is deliberately narrow: only the per-frame input the client owns. The render
// graph is server-owned -- the client may only SELECT one of the queues the server has announced
// (by name), never inject an arbitrary one. Outputs / render targets are the server's surface.
//

class SP_PUBLIC FrameRequestProxy : public Ref {
public:
	virtual ~FrameRequestProxy();

	// Pick, for this frame, one of the render queues the server has announced (by name).
	virtual void selectQueue(NotNull<core::Queue>) = 0;

	// Per-frame input the client owns (the command batch is the primary payload).
	virtual bool addInput(const AttachmentData *, Rc<AttachmentInputData> &&) = 0;

	// Submit one shared input to several attachments at once (dedup): local mode forwards the same
	// object to each; remote mode serializes it once and ships a single multi-key message. All listed
	// attachments must accept the same input type.
	virtual bool addInput(SpanView<const AttachmentData *>, Rc<AttachmentInputData> &&) = 0;

	virtual void addSignalDependency(Rc<DependencyEvent> &&) = 0;
	virtual void addSignalDependencies(Vector<Rc<DependencyEvent>> &&) = 0;
	virtual void addImageSpecialization(const ImageAttachment *, ImageInfoData &&) = 0;

	virtual const FrameConstraints &getFrameConstraints() const = 0;

	// Local: no-op. Remote: finalize + serialize the accumulated batch for transmission.
	virtual void commit() = 0;
};

// Single-process proxy: a transparent forward to the server's real FrameRequest (zero-copy).
class SP_PUBLIC LocalFrameRequestProxy final : public FrameRequestProxy {
public:
	virtual ~LocalFrameRequestProxy();

	bool init(NotNull<FrameRequest>);

	// The underlying server request (local-only; not part of the portable surface).
	FrameRequest *getRequest() const { return _request; }

	virtual void selectQueue(NotNull<core::Queue>) override;
	virtual bool addInput(const AttachmentData *, Rc<AttachmentInputData> &&) override;
	virtual bool addInput(SpanView<const AttachmentData *>, Rc<AttachmentInputData> &&) override;
	virtual void addSignalDependency(Rc<DependencyEvent> &&) override;
	virtual void addSignalDependencies(Vector<Rc<DependencyEvent>> &&) override;
	virtual void addImageSpecialization(const ImageAttachment *, ImageInfoData &&) override;
	virtual const FrameConstraints &getFrameConstraints() const override;
	virtual void commit() override;

protected:
	Rc<FrameRequest> _request;
};

// Networked proxy: accumulates the per-frame batch on the client, then serializes it for the
// server (which reconstructs a FrameRequest from the bytes and resolves the queue name against its
// registry).
//
// STAGE 2: SKELETON ONLY. The accumulation captures what would cross the wire, but the actual
// serialization (commit()) and the per-payload wire format (AttachmentInputData::serialize) are
// STUBS -- the real wire format, transport, and output/result delivery are later stages.
class SP_PUBLIC RemoteFrameRequestProxy final : public FrameRequestProxy {
public:
	virtual ~RemoteFrameRequestProxy();

	// Remote receives the frame constraints + the server-assigned frame id, plus transport hooks the
	// app layer injects (the proxy stays transport-agnostic): `sendInput` ships one already-serialized
	// per-attachment input immediately; `sendCommit` signals all inputs for this frame were sent.
	bool init(const FrameConstraints &, uint64_t frameId,
			Function<void(SpanView<const AttachmentData *>, BytesView)> &&sendInput,
			Function<void()> &&sendCommit);

	virtual void selectQueue(NotNull<core::Queue>) override;
	virtual bool addInput(const AttachmentData *, Rc<AttachmentInputData> &&) override;
	virtual bool addInput(SpanView<const AttachmentData *>, Rc<AttachmentInputData> &&) override;
	virtual void addSignalDependency(Rc<DependencyEvent> &&) override;
	virtual void addSignalDependencies(Vector<Rc<DependencyEvent>> &&) override;
	virtual void addImageSpecialization(const ImageAttachment *, ImageInfoData &&) override;
	virtual const FrameConstraints &getFrameConstraints() const override;
	virtual void commit() override;

	// The render-queue name the client selected for this frame (empty until selectQueue()). The
	// transport maps it back to the server's queue id.
	StringView getSelectedQueue() const { return _selectedQueue; }

protected:
	FrameConstraints _constraints;

	// The selected render-queue name (resolved against the server registry on the far side).
	String _selectedQueue;

	uint64_t _frameId = 0;
	Function<void(SpanView<const AttachmentData *>, BytesView)> _sendInput;
	Function<void()> _sendCommit;

	// Frame-level signal dependencies are accumulated but not yet shipped (cross-process dependency
	// wait/signal coordination is a later stage).
	Vector<Rc<DependencyEvent>> _signalDependencies;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREFRAMEREQUESTPROXY_H_ */
