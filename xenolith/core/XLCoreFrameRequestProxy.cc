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

bool RemoteFrameRequestProxy::init(const FrameConstraints &c) {
	_constraints = c;
	return true;
}

void RemoteFrameRequestProxy::selectQueue(NotNull<core::Queue>) {
	abort(); // TODO
	//_selectedQueue = name.str<Interface>();
}

bool RemoteFrameRequestProxy::addInput(const AttachmentData *a, Rc<AttachmentInputData> &&data) {
	_inputs.emplace_back(a, sp::move(data));
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
	// STUB (stage 2): the real implementation serializes _selectedQueue + _constraints + signal
	// deps + each accumulated input (via AttachmentInputData::serialize) into a CBOR buffer and
	// hands it to the transport; the server then reconstructs a FrameRequest. No transport exists
	// yet, so this is intentionally a no-op.
	log::source().warn("RemoteFrameRequestProxy",
			"commit(): remote frame serialization is not implemented yet (stage-2 stub)");
}

} // namespace stappler::xenolith::core
