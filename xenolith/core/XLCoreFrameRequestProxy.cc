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

#include "XLCoreFrameRequestProxy.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

// Out-of-line virtual destructors anchor the vtables in this single TU (stage-1 vtable lesson).
__SPRT_PUSH_ALLOW_CXXABI_ALLOC

FrameRequestProxy::~FrameRequestProxy() = default;

LocalFrameRequestProxy::~LocalFrameRequestProxy() = default;

RemoteFrameRequestProxy::~RemoteFrameRequestProxy() = default;

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool LocalFrameRequestProxy::init(NotNull<FrameRequest> req) {
	_request = req;
	return true;
}

void LocalFrameRequestProxy::selectQueue(NotNull<core::Queue> q) { _request->setQueue(q); }

bool LocalFrameRequestProxy::addInput(const AttachmentData *a, Rc<AttachmentInputData> &&data) {
	return _request->addInput(a, sp::move(data));
}

bool LocalFrameRequestProxy::addInput(SpanView<const AttachmentData *> atts,
		Rc<AttachmentInputData> &&data) {
	// Forward the same input object to each attachment (dedup is free locally).
	bool ret = true;
	for (auto a : atts) {
		ret = _request->addInput(a, Rc<AttachmentInputData>(data)) && ret;
	}
	return ret;
}

void LocalFrameRequestProxy::addSignalDependency(Rc<DependencyEvent> &&dep) {
	_request->addSignalDependency(sp::move(dep));
}

void LocalFrameRequestProxy::addSignalDependencies(Vector<Rc<DependencyEvent>> &&deps) {
	_request->addSignalDependencies(sp::move(deps));
}

void LocalFrameRequestProxy::addImageSpecialization(const ImageAttachment *a,
		ImageInfoData &&info) {
	_request->addImageSpecialization(a, sp::move(info));
}

const FrameConstraints &LocalFrameRequestProxy::getFrameConstraints() const {
	return _request->getFrameConstraints();
}

void LocalFrameRequestProxy::commit() {
	// Local mode: nothing to do -- the client wrote straight into the server's FrameRequest.
}

// --- RemoteFrameRequestProxy (skeleton; serialization is a stub this stage) ---

bool RemoteFrameRequestProxy::init(const FrameConstraints &c, uint64_t frameId,
		Function<void(SpanView<const AttachmentData *>, BytesView)> &&sendInput,
		Function<void()> &&sendCommit) {
	_constraints = c;
	_frameId = frameId;
	_sendInput = sp::move(sendInput);
	_sendCommit = sp::move(sendCommit);
	return true;
}

void RemoteFrameRequestProxy::selectQueue(NotNull<core::Queue> q) {
	// The client may only pick one of the server-announced queues; store its name (the server
	// resolves it back against its registry). The mirror queue's name equals the server's.
	_selectedQueue = q->getName().str<Interface>();
}

bool RemoteFrameRequestProxy::addInput(const AttachmentData *a, Rc<AttachmentInputData> &&data) {
	return addInput(makeSpanView(&a, 1), sp::move(data));
}

bool RemoteFrameRequestProxy::addInput(SpanView<const AttachmentData *> atts,
		Rc<AttachmentInputData> &&data) {
	// Stream the input the moment it is submitted: serialize it once (the concrete input owns its wire
	// format) and hand the bytes to the transport addressed to every target attachment.
	if (atts.empty() || !data || !_sendInput) {
		return false;
	}
	Bytes bytes;
	if (!data->serialize(
				[&](BytesView v) { bytes.insert(bytes.end(), v.data(), v.data() + v.size()); })) {
		return false;
	}
	_sendInput(atts, bytes);
	return true;
}

void RemoteFrameRequestProxy::addSignalDependency(Rc<DependencyEvent> &&dep) {
	_signalDependencies.emplace_back(sp::move(dep));
}

void RemoteFrameRequestProxy::addSignalDependencies(Vector<Rc<DependencyEvent>> &&deps) {
	for (auto &it : deps) { _signalDependencies.emplace_back(sp::move(it)); }
}

void RemoteFrameRequestProxy::addImageSpecialization(const ImageAttachment *, ImageInfoData &&) {
	// TODO(remote stage): accumulate image specialization for serialization.
}

const FrameConstraints &RemoteFrameRequestProxy::getFrameConstraints() const {
	return _constraints;
}

void RemoteFrameRequestProxy::commit() {
	// Inputs were already streamed as they arrived; commit only signals "all inputs sent" so the
	// server can stop expecting more for this frame.
	if (_sendCommit) {
		_sendCommit();
	}
}

} // namespace stappler::xenolith::core
