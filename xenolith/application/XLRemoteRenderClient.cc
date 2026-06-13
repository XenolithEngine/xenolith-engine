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

#include "XLRemoteRenderClient.h"
#include "XLRemoteSerialize.h"
#include "XLRemoteProtocol.h"
#include "XLServerAppThread.h"
#include "XLCoreFrameRequestProxy.h"
#include "XLCoreAttachment.h" // complete core::Attachment for makeInputData()
#include "XLCoreLoop.h" // gapi loop performOnThread for frame-input submission

#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

RemoteRenderClient::~RemoteRenderClient() = default;

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool RemoteRenderClient::init(NotNull<ServerAppThread> host, Rc<remote::ServerConnection> &&conn) {
	_host = host;
	_connection = sp::move(conn);
	return _connection != nullptr;
}

bool RemoteRenderClient::isClosed() { return !_connection || _connection->isClosed(); }

void RemoteRenderClient::closeConnection() {
	if (_connection) {
		_connection->close(); // graceful QUIC shutdown (bounded); then drop it
		_connection = nullptr;
	}
	_pendingFrames.clear();
}

void RemoteRenderClient::announce(NotNull<remote::ObjectRegistry> registry) {
	Value data;
	auto &windows = data.emplace("windows");
	for (auto &it : registry->getWindows()) {
		auto &v = windows.emplace();
		v.addInteger(it.first);
		v.addString(it.second.window->getId());
		v.addInteger(toInt(it.second.window->getWindowState()));
		v.addInteger(toInt(it.second.window->getCapabilities()));
		v.addValue(remote::serializeFrameConstraints(it.second.window->getConstraints()));
		v.addValue(remote::serializeSwapchainConfig(it.second.window->getAppSwapchainConfig()));

		Value &queues = v.emplace();
		for (auto &qIt : it.second.queues) {
			auto q = registry->resolveQueue(qIt);
			if (q) {
				auto &v = queues.emplace();
				v.addInteger(qIt);
				v.addString(q->queue->getName());
			}
		}

		if (auto info = it.second.window->getInfo()) {
			v.addValue(remote::serializeWindowInfo(*info));
		}
	}

	_connection->sendCborMessage(remote::Domain::Global,
			toInt(remote::GlobalCode::SharedObjectsAnnounce), data);
}

void RemoteRenderClient::acquireFrame(uint64_t windowId, NotNull<core::FrameRequestProxy> proxy,
		Function<void(bool)> &&cb) {
	auto registry = _host ? _host->getSharedObjects() : nullptr;
	if (!registry || isClosed() || windowId == 0) {
		cb(false);
		return;
	}

	auto frameId = _nextFrameId++;

	Value req;
	req.addInteger(int64_t(frameId));
	req.addInteger(int64_t(windowId));
	req.addValue(remote::serializeFrameConstraints(proxy->getFrameConstraints()));

	// Hold the completion across the async reply and guarantee it fires exactly once (on reply or on
	// send failure) without a use-after-move on `cb`.
	struct FrameReply : Ref {
		Function<void(bool)> cb;
	};
	auto pending = Rc<FrameReply>::alloc();
	pending->cb = sp::move(cb);

	// The server always wraps its real FrameRequest in a LocalFrameRequestProxy; keep it alive to
	// route streamed input until the frame commits.
	auto localProxy = Rc<core::LocalFrameRequestProxy>(
			static_cast<core::LocalFrameRequestProxy *>(proxy.get()));

	auto sent = _host->sendMessageWithReply(remote::Domain::Window,
			toInt(remote::WindowCode::AcquireFrame), req,
			[this, pending, frameId, localProxy](const remote::MessageHeader &h, BytesView payload) {
		if (remote::isError(h)) {
			log::source().warn("RemoteRenderClient", "AcquireFrame ", frameId, " rejected (code ",
					uint32_t(h.code), ")");
			pending->cb(false);
			return;
		}

		auto val = data::read<Interface>(payload);
		auto queueId = uint64_t(val.getInteger(1));
		auto sq = _host->getSharedObjects()->resolveQueue(queueId);
		if (!sq) {
			log::source().warn("RemoteRenderClient", "AcquireFrame ", frameId,
					" reply selected unknown queue id ", queueId);
			pending->cb(false);
			return;
		}

		// Arm the request with the client's selected queue and keep it routable for streamed input
		// (FrameInput messages) until the matching FrameCommit.
		log::source().info("RemoteRenderClient", "AcquireFrame ", frameId, " -> queue '",
				sq->queue->getName(), "' (id ", queueId, ")");
		localProxy->selectQueue(sq->queue);
		_pendingFrames.emplace(frameId, localProxy);
		pending->cb(true);
	});

	if (!sent) {
		pending->cb(false);
	}
}

void RemoteRenderClient::handleFrameInput(uint64_t frameId, SpanView<StringView> attachmentKeys,
		BytesView bytes) {
	auto it = _pendingFrames.find(frameId);
	if (it == _pendingFrames.end()) {
		return;
	}
	auto req = Rc<core::FrameRequest>(it->second->getRequest());
	if (!req) {
		return;
	}
	auto &queue = req->getQueue();
	if (!queue) {
		return;
	}

	// Resolve every target attachment; deserialize the shared payload once (the first attachment mints
	// the concrete input type -- all keys in a multi-key message accept the same type).
	Vector<const core::AttachmentData *> atts;
	Rc<core::AttachmentInputData> input;
	for (auto key : attachmentKeys) {
		auto attData = queue->getAttachment(key);
		if (!attData || !attData->attachment) {
			log::source().warn("RemoteRenderClient", "FrameInput ", frameId,
					" for unknown attachment '", key, "'");
			continue;
		}
		if (!input) {
			input = attData->attachment->makeInputData();
		}
		atts.emplace_back(attData);
	}
	if (atts.empty() || !input || !input->deserialize(bytes)) {
		log::source().warn("RemoteRenderClient", "FrameInput ", frameId,
				" failed to reconstruct input");
		return;
	}

	// Submit on the gapi loop thread (where the frame queue runs), mirroring the local renderer; the
	// one input object is shared across all its attachments.
	if (auto loop = _host->getGlLoop()) {
		loop->performOnThread([req, atts = sp::move(atts), input = sp::move(input)]() mutable {
			for (auto a : atts) { req->addInput(a, Rc<core::AttachmentInputData>(input)); }
		});
	}
}

void RemoteRenderClient::handleFrameCommit(uint64_t frameId) { _pendingFrames.erase(frameId); }

void RemoteRenderClient::handleRenderQueueAttached(const Rc<core::Queue> &) { }
void RemoteRenderClient::handleConstraintsChanged(const core::FrameConstraints &) { }
void RemoteRenderClient::handleInputEvents(Vector<core::InputEventData> &&) { }
void RemoteRenderClient::handleTextInput(const core::TextInputState &) { }
void RemoteRenderClient::handleFramePresented(uint64_t) { }

void RemoteRenderClient::handleMaterialsUpdated(uint64_t queue, NotNull<core::MaterialSet> set,
		NotNull<remote::ObjectRegistry> registry) {
	if (!_connection || _connection->isClosed()) {
		return;
	}

	auto data = remote::QueueCodec::encodeMaterials(queue, *set, *registry);
	if (data.empty()) {
		return;
	}

	_connection->sendMessage(remote::Domain::Window, toInt(remote::WindowCode::UpdateMaterials),
			data);
}

} // namespace stappler::xenolith
