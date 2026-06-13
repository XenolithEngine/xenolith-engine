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

#include "XLRemoteWindow.h"
#include "XLRemoteSerialize.h"
#include "XLClientAppThread.h"
#include "XLCoreFrameRequestProxy.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

RemoteWindow::~RemoteWindow() { }

bool RemoteWindow::init(NotNull<ClientAppThread> thread, const Value &val) {
	_id = val.getInteger(0);
	_windowId = val.getString(1);
	_state = static_cast<core::WindowState>(val.getInteger(2));
	_capabilities = static_cast<sprt::window::WindowCapabilities>(val.getInteger(3));
	_appFrameConstraints = remote::deserializeFrameConstraints(val.getValue(4));
	_appSwapchainConfig = remote::deserializeSwapchainConfig(val.getValue(5));

	for (auto &qIt : val.getValue(6).asArray()) {
		if (qIt.isArray() && qIt.size() == 2) {
			_queues.emplace_back(RemoteQueueInfo{
				static_cast<uint64_t>(qIt.getInteger(0)),
				qIt.getString(1),
			});
		}
	}

	if (val.hasValue(7)) {
		_info = remote::deserializeWindowInfo(val.getValue(6));
	}

	if (_queues.empty()) {
		slog().warn("RemoteWindow", "No shared queues for a window, it's unusable as shared");
		return false;
	}

	_thread = thread;
	return true;
}

void RemoteWindow::compileRenderQueue(const Rc<core::Queue> &q, Function<void(bool)> &&cb) {
	auto n = q->getName();
	uint64_t id = 0;
	for (auto &it : _queues) {
		if (it.name == n) {
			id = it.id;
			break;
		}
	}
	if (id == 0) {
		slog().error("RemoteWindow", "No queue named '", n, "' found for a shared window");
		cb(false);
		return;
	}

	// now, we should send server a note that queue is requested

	auto c = _thread->getConnection();
	if (!c) {
		slog().error("RemoteWindow", "Not connected");
		cb(false);
		return;
	}

	if (!_thread->sendMessageWithReply(remote::Domain::Window,
				toInt(remote::WindowCode::CompileQueue), Value(id),
				[this, cb = sp::move(cb), q, id](const remote::MessageHeader &h,
						BytesView payload) {
		if (remote::isError(h)) {
			slog().error("RemoteWindow", "compileRenderQueue: network failure: ", int(h.code));
			cb(false);
			return;
		}

		auto sq = _thread->getSharedObjects()->makeQueue(id, *q, payload);
		if (sq == q) {
			cb(true);
		} else {
			cb(false);
		}
	})) {
		cb(false);
	}
}

void RemoteWindow::acquireFrame(uint64_t frameId, const core::FrameConstraints &c,
		Function<void(uint64_t queueId)> &&reply) {
	// `_client` is this window's local Director (set via RenderServerChannel::setRenderClient).
	if (!_client) {
		reply(0);
		return;
	}

	// Stream each per-attachment input the moment the scene submits it, then a commit. These run on
	// the client app thread (Director::performOnRenderThread resolves there), so the connection is
	// touched on its owning thread.
	auto thread = _thread;
	auto proxy = Rc<core::RemoteFrameRequestProxy>::create(c, frameId,
			[thread, frameId](SpanView<const core::AttachmentData *> atts, BytesView bytes) {
		if (auto conn = thread->getConnection()) {
			// [frameId, keys[], bytes] -- one serialized input addressed to multiple attachments.
			Value msg;
			msg.addInteger(int64_t(frameId));
			auto &keys = msg.emplace();
			for (auto a : atts) { keys.addString(a->key); }
			msg.addBytes(bytes);
			conn->sendCborMessage(remote::Domain::Window, toInt(remote::WindowCode::FrameInput), msg);
		}
	},
			[thread, frameId]() {
		if (auto conn = thread->getConnection()) {
			Value msg;
			msg.addInteger(int64_t(frameId));
			conn->sendCborMessage(remote::Domain::Window, toInt(remote::WindowCode::FrameCommit), msg);
		}
	});
	if (!proxy) {
		reply(0);
		return;
	}

	// Director::acquireFrame sets selectQueue() synchronously and fires this callback before its async
	// render/commit, so the selected queue is already populated here. Per-frame input is deferred.
	_client->acquireFrame(0, proxy, [this, proxy, reply = sp::move(reply)](bool ok) mutable {
		if (!ok) {
			reply(0);
			return;
		}

		// Map the selected queue name back to the server's shared queue id.
		auto name = proxy->getSelectedQueue();
		uint64_t id = 0;
		for (auto &q : _queues) {
			if (q.name == name) {
				id = q.id;
				break;
			}
		}
		reply(id);
	});
}

void RemoteWindow::compileResource(Rc<core::Resource> &&, Function<void(bool)> &&, bool preload) { }
void RemoteWindow::compileMaterials(Rc<core::MaterialInputData> &&,
		const Vector<Rc<core::DependencyEvent>> &) { }
void RemoteWindow::compileImage(const Rc<core::DynamicImage> &, Function<void(bool)> &&) { }

void RemoteWindow::attachRenderQueue(const Rc<core::Queue> &) {
	// Here we should tell window to use this client queue for drawing
}

void RemoteWindow::setReadyForNextFrame() { }
void RemoteWindow::setPreferredFrameInterval(uint64_t intervalUs) { }
core::FrameTimingInfo RemoteWindow::getFrameTiming() const { return core::FrameTimingInfo(); }

void RemoteWindow::acquireScreenInfo(Function<void(NotNull<core::ScreenInfo>)> &&, Ref *) { }
void RemoteWindow::acquireTextInput(core::TextInputRequest &&) { }
void RemoteWindow::releaseTextInput() { }
void RemoteWindow::close(bool graceful) { }

void RemoteWindow::handleBackButton() { }

const sprt::window::WindowInfo *RemoteWindow::getInfo() const { return _info; }

bool RemoteWindow::enableState(core::WindowState) { return false; }
bool RemoteWindow::disableState(core::WindowState) { return false; }

bool RemoteWindow::setFullscreen(core::FullscreenInfo &&, Function<void(Status)> &&, Ref *) {
	return false;
}

bool RemoteWindow::setPreferredFrameRate(float, Function<void(Status)> &&) { return false; }

void RemoteWindow::captureScreenshot(
		Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) { }

bool RemoteWindow::openWindowMenu(Vec2 pos) { return false; }

void RemoteWindow::handleInputEvents(Vector<core::InputEventData> &&events) { }

void RemoteWindow::updateLayers(sprt::window::Vector<sprt::window::WindowLayer> &&) { }

} // namespace stappler::xenolith
