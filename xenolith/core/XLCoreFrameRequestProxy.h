/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

// Resolves a render-queue name to the server's compiled Queue. Implemented by the server endpoint
// (AppWindow keeps the name -> Rc<Queue> registry of its compiled render graphs). The local proxy
// uses it to resolve selectQueue(); the remote server resolves a serialized name the same way.
class SP_PUBLIC RenderQueueRegistry {
public:
	virtual ~RenderQueueRegistry();

	virtual Rc<Queue> getRenderQueue(StringView name) const = 0;
};

class SP_PUBLIC FrameRequestProxy : public Ref {
public:
	virtual ~FrameRequestProxy();

	// Pick, for this frame, one of the render queues the server has announced (by name).
	virtual void selectQueue(StringView name) = 0;

	// Per-frame input the client owns (the command batch is the primary payload).
	virtual bool addInput(const AttachmentData *, Rc<AttachmentInputData> &&) = 0;
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

	bool init(NotNull<FrameRequest>, NotNull<RenderQueueRegistry>);

	// The underlying server request (local-only; not part of the portable surface).
	FrameRequest *getRequest() const { return _request; }

	virtual void selectQueue(StringView name) override;
	virtual bool addInput(const AttachmentData *, Rc<AttachmentInputData> &&) override;
	virtual void addSignalDependency(Rc<DependencyEvent> &&) override;
	virtual void addSignalDependencies(Vector<Rc<DependencyEvent>> &&) override;
	virtual void addImageSpecialization(const ImageAttachment *, ImageInfoData &&) override;
	virtual const FrameConstraints &getFrameConstraints() const override;
	virtual void commit() override;

protected:
	Rc<FrameRequest> _request;
	RenderQueueRegistry *_registry = nullptr;
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

	// Remote receives the frame constraints from the server's per-frame announcement.
	bool init(const FrameConstraints &);

	virtual void selectQueue(StringView name) override;
	virtual bool addInput(const AttachmentData *, Rc<AttachmentInputData> &&) override;
	virtual void addSignalDependency(Rc<DependencyEvent> &&) override;
	virtual void addSignalDependencies(Vector<Rc<DependencyEvent>> &&) override;
	virtual void addImageSpecialization(const ImageAttachment *, ImageInfoData &&) override;
	virtual const FrameConstraints &getFrameConstraints() const override;
	virtual void commit() override;

protected:
	FrameConstraints _constraints;

	// The selected render-queue name (resolved against the server registry on the far side).
	String _selectedQueue;

	// Accumulated batch that would be serialized. NOTE: attachments are kept by pointer here only
	// as a placeholder; the real wire format must key them by a stable id/name, since pointers do
	// not survive a process boundary.
	Vector<Pair<const AttachmentData *, Rc<AttachmentInputData>>> _inputs;
	Vector<Rc<DependencyEvent>> _signalDependencies;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREFRAMEREQUESTPROXY_H_ */
