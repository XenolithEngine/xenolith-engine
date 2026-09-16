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
#include "XLRemoteProtocol.h"
#include "XLClientAppThread.h"
#include "XLCoreFrameRequestProxy.h"
#include "XLCoreMaterial.h" // forward runtime material compiles to the server
#include "XLCoreAttachment.h" // DependencyEvent ids

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Per-request reply deadlines (relative us). CompileQueue compiles a render graph on the server,
// so it is generous; AttachQueue is a trivial ack. On timeout the watchdog fails the waiter and
// the client disconnects.
static constexpr uint64_t kCompileQueueReplyTimeoutUs = 15'000'000; // 15s
static constexpr uint64_t kAttachQueueReplyTimeoutUs = 5'000'000; // 5s

// A window-control op is a short round trip on the server's app thread.
static constexpr uint64_t kWindowControlReplyTimeoutUs = 5'000'000; // 5s

RemoteWindow::~RemoteWindow() { }

bool RemoteWindow::init(NotNull<ClientAppThread> thread, const Value &val) {
	_id = val.getInteger(0);
	_windowId = val.getString(1);
	_state = static_cast<core::WindowState>(val.getInteger(2));
	_capabilities = static_cast<sprt::window::WindowCapabilities>(val.getInteger(3));
	_appFrameConstraints = remote::deserializeFrameConstraints(val.getValue(4));
	_appSwapchainConfig = remote::deserializeSwapchainConfig(val.getValue(5));

	for (auto &qIt : val.getValue(6).asArray()) {
		// Entries are [id, name] (version-1) or [id, name, api, typeTag, damage]; a short entry
		// leaves the descriptive fields at their defaults.
		if (qIt.isArray() && qIt.size() >= 2) {
			_queues.emplace_back(RemoteQueueInfo{
				static_cast<uint64_t>(qIt.getInteger(0)),
				qIt.getString(1),
				core::InstanceApi(qIt.getInteger(2)),
				static_cast<uint32_t>(qIt.getInteger(3)),
				core::QueueDamageFlags(qIt.getInteger(4)),
			});
		}
	}

	// Index 6 is the queue array, the WindowInfo is at 7 (see RemoteRenderClient::announce). The
	// slot is always present (empty when the window has no info), so check the type, not
	// hasValue().
	if (val.getValue(7).isArray()) {
		_info = remote::deserializeWindowInfo(val.getValue(7));
	}

	// [8] Geometry at connect time; absent from a version-1 server, leaving the mirror at defaults.
	if (val.getValue(8).isArray()) {
		_appWindowGeometry = remote::deserializeWindowGeometry(val.getValue(8));
	}

	// [9] The CreateWindow request this window answers, or 0 -- including from a server that does
	// not emit the slot at all.
	_creatorSerial = uint32_t(val.getValue(9).getInteger());

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

	slog().debug("RemoteWindow", "compileRenderQueue: request");

	if (!_thread->sendMessageWithReply(remote::Domain::Window,
				toInt(remote::WindowCode::CompileQueue), Value(id),
				[this, cb = sp::move(cb), q, id](const remote::MessageHeader &h,
						BytesView payload) {
		if (remote::isError(h)) {
			slog().error("RemoteWindow", "compileRenderQueue: network failure: ", int(h.code));
			cb(false);
			return;
		}

		slog().debug("RemoteWindow", "compileRenderQueue: reply");

		auto sq = _thread->getSharedObjects()->makeQueue(id, *q, payload);
		if (sq == q) {
			cb(true);
		} else {
			cb(false);
		}
	},
				kCompileQueueReplyTimeoutUs)) {
		cb(false);
	}
}

void RemoteWindow::acquireFrame(uint64_t frameId, const core::FrameConstraints &c,
		const core::FrameTimingInfo *timing, const core::DrawStat *stat,
		Function<void(uint64_t queueId)> &&reply) {
	// `_client` is this window's local Director (set via RenderServerChannel::setRenderClient).
	if (!_client) {
		slog().error("RemoteWindow", "acquireFrame: no client");
		reply(0);
		return;
	}

	/* The window's constraints mirror is written only here after the announce: constraints arrive
	with every frame request, so the mirror catches up with the first frame after a resize. */
	if (_appFrameConstraints != c) {
		_appFrameConstraints = c;
		_client->handleConstraintsChanged(c);
	}

	// Telemetry that rode along with the request; absent means no update, keep the previous value.
	if (timing) {
		_frameTiming = *timing;
	}
	if (stat) {
		// Straight to the Director, which owns the copy the FPS overlay reads; already on the app
		// thread.
		_client->pushDrawStat(0, *stat);
	}

	// Stream each per-attachment input as the scene submits it, then a commit. These run on the
	// client app thread (Director::performOnRenderThread resolves there), which owns the
	// connection.
	auto thread = _thread;
	auto proxy = Rc<core::RemoteFrameRequestProxy>::create(c, frameId,
			[thread, frameId](SpanView<const core::AttachmentData *> atts, BytesView bytes) {
		if (auto conn = thread->getConnection()) {
			// Flush pending glyph requests before this frame's FrameInput, so the server registers
			// the gating dependency before reconciling the frame against it.
			thread->flushPendingFontGlyphs();

			// [frameId, keys[], bytes] -- one serialized input addressed to multiple attachments.
			Value msg;
			msg.addInteger(int64_t(frameId));
			auto &keys = msg.emplace();
			for (auto a : atts) { keys.addString(a->key); }
			msg.addBytes(bytes);
			conn->sendCborMessage(remote::Domain::Window, toInt(remote::WindowCode::FrameInput),
					msg);
		}
	}, [thread, frameId]() {
		if (auto conn = thread->getConnection()) {
			Value msg;
			msg.addInteger(int64_t(frameId));
			conn->sendCborMessage(remote::Domain::Window, toInt(remote::WindowCode::FrameCommit),
					msg);
		}
	});
	if (!proxy) {
		slog().error("RemoteWindow", "acquireFrame: fail to create frame proxy");
		reply(0);
		return;
	}

	// Director::acquireFrame calls selectQueue() and fires this callback before its async
	// render/commit, so the selected queue is set here. Per-frame input is deferred.
	_client->acquireFrame(0, proxy, [this, proxy, reply = sp::move(reply)](bool ok) mutable {
		if (!ok) {
			slog().error("RemoteWindow", "acquireFrame: fail to acquire director's frame");
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
		// slog().debug("RemoteWindow", "acquireFrame: queue: ", id);
		reply(id);
	});
}

void RemoteWindow::compileResource(Rc<core::Resource> &&, Function<void(bool)> &&, bool preload) { }

void RemoteWindow::compileMaterials(Rc<core::MaterialInputData> &&req,
		const Vector<Rc<core::DependencyEvent>> &deps) {
	// The headless client can not compile a runtime material (no GPU). The server resolves image
	// refs, compiles into the window's MaterialSet under the client-assigned ids, signals the
	// gating deps, and pushes the set back.
	auto conn = _thread->getConnection();
	if (!conn || !req) {
		return;
	}

	slog().info("RemoteWindow", "compileMaterials: forwarding ", req->materialsToAddOrUpdate.size(),
			" material(s), ", deps.size(), " dep(s)");

	Value msg;
	msg.setInteger(int64_t(_id), "window");

	Value depv;
	for (auto &d : deps) {
		if (d) {
			depv.addInteger(int64_t(d->getId()));
		}
	}
	msg.setValue(sp::move(depv), "deps");

	Value mats;
	for (auto &m : req->materialsToAddOrUpdate) {
		Value mv;
		mv.setInteger(int64_t(m->getId()), "id");
		mv.setString(m->getPipeline() ? StringView(m->getPipeline()->key) : StringView(), "pl");
		Value imgs;
		for (auto &mi : m->getImages()) { imgs.addValue(remote::serializeMaterialImage(mi)); }
		mv.setValue(sp::move(imgs), "imgs");
		mats.addValue(sp::move(mv));
	}
	msg.setValue(sp::move(mats), "mats");

	// dynamicMaterialsToUpdate / materialsToRemove are not forwarded (atlas growth is tracked
	// server-side via the dynamic material).
	conn->sendCborMessage(remote::Domain::Window, toInt(remote::WindowCode::CompileMaterials), msg);
}

void RemoteWindow::compileImage(const Rc<core::DynamicImage> &, Function<void(bool)> &&) { }

void RemoteWindow::attachRenderQueue(const Rc<core::Queue> &) {
	// The shared queue is now the Director's active render graph. Send AttachQueue: only then does
	// the server route the window's frames here. The server replies with an empty acknowledgement.
	auto c = _thread->getConnection();
	if (!c) {
		slog().error("RemoteWindow", "attachRenderQueue: not connected");
		return;
	}

	slog().debug("RemoteWindow", "attachRenderQueue: signal ready");

	if (!_thread->sendMessageWithReply(remote::Domain::Window,
				toInt(remote::WindowCode::AttachQueue), Value(_id),
				[](const remote::MessageHeader &h, BytesView) {
		if (remote::isError(h)) {
			slog().error("RemoteWindow", "attachRenderQueue: server rejected (code ", int(h.code),
					")");
			return;
		}
		slog().debug("RemoteWindow", "attachRenderQueue: server switched to client");
	}, kAttachQueueReplyTimeoutUs)) {
		slog().error("RemoteWindow", "attachRenderQueue: failed to send ready signal");
	}
}

void RemoteWindow::setReadyForNextFrame() {
	// The client's Director has active actions/input and wants another frame; ask the server's
	// PresentationEngine to schedule it. Fire-and-forget.
	if (auto conn = _thread->getConnection()) {
		conn->sendCborMessage(remote::Domain::Window, toInt(remote::WindowCode::ReadyForNextFrame),
				Value(_id));
	}
}
/* --- window control (WindowCode::WindowControl) ------------------------------------------------

The returned `bool` is a precondition, answered locally from the `_state` and `_capabilities`
mirrors with the shared RenderServerChannel rules. The `Status` in the reply is the outcome: the
server re-checks the precondition and answers Declined if it disagrees. */

void RemoteWindow::sendWindowControl(remote::WindowControlOp op, Value &&args,
		Function<void(Status)> &&cb) {
	args.setInteger(int64_t(_id), "w");
	args.setInteger(int64_t(toInt(op)), "op");

	if (!_thread
			|| !_thread->sendMessageWithReply(remote::Domain::Window,
					toInt(remote::WindowCode::WindowControl), args,
					[cb = sp::move(cb)](const remote::MessageHeader &h, BytesView payload) mutable {
		if (!cb) {
			return;
		}
		if (remote::isError(h)) {
			cb(Status::ErrorNotSupported);
			return;
		}
		cb(Status(int32_t(data::read<Interface>(payload).getInteger(0))));
	}, kWindowControlReplyTimeoutUs)) {
		// Not connected, or the send failed: answer, so the caller does not wait forever.
		if (cb) {
			cb(Status::ErrorNotSupported);
		}
	}
}

void RemoteWindow::setPreferredFrameInterval(uint64_t intervalUs) {
	Value args;
	args.setInteger(int64_t(intervalUs), "iv");
	sendWindowControl(remote::WindowControlOp::SetPreferredFrameInterval, sp::move(args), nullptr);
}

core::FrameTimingInfo RemoteWindow::getFrameTiming() const { return _frameTiming; }

void RemoteWindow::acquireScreenInfo(Function<void(NotNull<core::ScreenInfo>)> &&cb, Ref *) {
	// Screen enumeration is not supported remotely: deliver an empty ScreenInfo rather than
	// dropping the callback.
	if (cb) {
		cb(Rc<core::ScreenInfo>::alloc());
	}
}

/* --- text input (WindowCode::TextInputControl / ::TextInputState) --------------------------------

The application only requests a state; the IME owns it. The local TextInputManager is updated only
by the echo in handleTextInput, never optimistically. */

void RemoteWindow::acquireTextInput(core::TextInputRequest &&req) {
	Value args;
	args.setInteger(int64_t(toInt(remote::TextInputOp::Acquire)), "op");
	args.setValue(remote::serializeTextInputRequest(req), "req");
	sendTextInputControl(sp::move(args));
}

void RemoteWindow::releaseTextInput() {
	Value args;
	args.setInteger(int64_t(toInt(remote::TextInputOp::Release)), "op");
	sendTextInputControl(sp::move(args));
}

void RemoteWindow::performTextInput(core::TextInputCommand &&cmd) {
	Value args;
	args.setInteger(int64_t(toInt(remote::TextInputOp::Perform)), "op");
	args.setValue(remote::serializeTextInputCommand(cmd), "cmd");
	sendTextInputControl(sp::move(args));
}

void RemoteWindow::sendTextInputControl(Value &&args) {
	auto conn = _thread ? _thread->getConnection() : nullptr;
	if (!conn) {
		return;
	}
	args.setInteger(int64_t(_id), "w");
	conn->sendCborMessage(remote::Domain::Window, toInt(remote::WindowCode::TextInputControl),
			args);
}

void RemoteWindow::handleTextInput(const core::TextInputState &state) {
	// The echo, straight to the Director's TextInputManager, as in AppWindow::handleTextInput.
	if (_client) {
		_client->handleTextInput(0, state);
	}
}

void RemoteWindow::close(bool graceful) {
	Value args;
	args.setBool(graceful, "graceful");
	sendWindowControl(remote::WindowControlOp::Close, sp::move(args), nullptr);
}

void RemoteWindow::handleBackButton() {
	sendWindowControl(remote::WindowControlOp::BackButton, Value(), nullptr);
}

const sprt::window::WindowInfo *RemoteWindow::getInfo() const { return _info; }

bool RemoteWindow::enableState(core::WindowState state) {
	if (!validateStateChange(state, "enableState")) {
		return false;
	}
	Value args;
	args.setInteger(int64_t(toInt(state)), "state");
	sendWindowControl(remote::WindowControlOp::EnableState, sp::move(args), nullptr);
	return true;
}

bool RemoteWindow::disableState(core::WindowState state) {
	if (!validateStateChange(state, "disableState")) {
		return false;
	}
	Value args;
	args.setInteger(int64_t(toInt(state)), "state");
	sendWindowControl(remote::WindowControlOp::DisableState, sp::move(args), nullptr);
	return true;
}

bool RemoteWindow::setFullscreen(core::FullscreenInfo &&info, Function<void(Status)> &&cb, Ref *) {
	// Same gate as AppWindow, from the mirrored capabilities; a refusal still answers the callback.
	if (!canSetFullscreen()) {
		if (cb) {
			cb(Status::ErrorNotSupported);
		}
		return false;
	}
	Value args;
	args.setValue(remote::serializeFullscreenInfo(info), "fs");
	sendWindowControl(remote::WindowControlOp::SetFullscreen, sp::move(args), sp::move(cb));
	return true;
}

bool RemoteWindow::setPreferredFrameRate(float rate, Function<void(Status)> &&cb) {
	// Returns true unconditionally, matching AppWindow (which does not gate on
	// WindowCapabilities::PreferredFrameRate either).
	Value args;
	args.setDouble(rate, "rate");
	sendWindowControl(remote::WindowControlOp::SetPreferredFrameRate, sp::move(args), sp::move(cb));
	return true;
}

void RemoteWindow::setWindowExtent(Extent2 extent, Function<void(Status)> &&cb, Ref *) {
	Value args;
	auto &ext = args.emplace("ext");
	ext.addInteger(int64_t(extent.width));
	ext.addInteger(int64_t(extent.height));
	sendWindowControl(remote::WindowControlOp::SetWindowExtent, sp::move(args), sp::move(cb));
}

void RemoteWindow::captureScreenshot(
		Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) {
	// The headless client can not render. The server captures the pixels and returns them as a
	// Domain::Data Screenshot transfer (see ClientAppThread::loadExtensions -> deliverScreenshot),
	// matched to `cb` by the RequestScreenshot serial in the announce reason. Fire-and-forget.
	auto conn = _thread ? _thread->getConnection() : nullptr;
	if (!conn) {
		slog().error("RemoteWindow", "captureScreenshot: not connected");
		if (cb) {
			cb(core::ImageInfoData(), BytesView());
		}
		return;
	}

	uint32_t serial = 0;
	if (conn->sendCborMessage(remote::Domain::Window, toInt(remote::WindowCode::RequestScreenshot),
				Value(_id), &serial)
			!= remote::GlobalError::Ok) {
		slog().error("RemoteWindow", "captureScreenshot: failed to send request");
		if (cb) {
			cb(core::ImageInfoData(), BytesView());
		}
		return;
	}

	_pendingScreenshots.emplace(serial, sp::move(cb));
	slog().debug("RemoteWindow", "captureScreenshot: requested (serial ", serial, ")");
}

bool RemoteWindow::deliverScreenshot(uint32_t serial, const core::ImageInfoData &info,
		BytesView pixels) {
	auto it = _pendingScreenshots.find(serial);
	if (it == _pendingScreenshots.end()) {
		return false;
	}
	auto cb = sp::move(it->second);
	_pendingScreenshots.erase(it);
	slog().debug("RemoteWindow", "deliverScreenshot: ", pixels.size(), " bytes for serial ",
			serial);
	if (cb) {
		cb(info, pixels);
	}
	return true;
}

bool RemoteWindow::openWindowMenu(Vec2 pos) {
	if (!canOpenWindowMenu()) {
		return false;
	}
	Value args;
	auto &p = args.emplace("pos");
	p.addDouble(pos.x);
	p.addDouble(pos.y);
	sendWindowControl(remote::WindowControlOp::OpenWindowMenu, sp::move(args), nullptr);
	return true;
}

void RemoteWindow::handleInputEvents(Vector<core::InputEventData> &&events) {
	// Server-forwarded platform input (WindowCode::InputEvents): replay it into the local Director
	// on the app thread, as a real window does. Window-state events also update the mirrored state.
	for (auto &event : events) {
		if (event.event == core::InputEventName::WindowState) {
			_state = event.window.state;
		}
	}
	if (_client) {
		_client->handleInputEvents(0, sp::move(events));
	}
}

void RemoteWindow::handleWindowGeometryChanged(const sprt::window::WindowGeometry &g) {
	// The server pushes this only on a real change (AppWindow::notifyWindowGeometry compares
	// first). Update the mirror, then notify the scene through the same hook a local window uses.
	_appWindowGeometry = g;
	if (_client) {
		_client->handleWindowGeometryChanged(0, g);
	}
}

void RemoteWindow::updateLayers(sprt::window::Vector<sprt::window::WindowLayer> &&layers) {
	// The client's InputDispatcher computes the interaction layers (hit/cursor/drag regions), but
	// the server owns the OS window: forward them in the typed wire format (see
	// serializeWindowLayers).

	auto conn = _thread ? _thread->getConnection() : nullptr;
	if (!conn) {
		return;
	}

	Bytes blob;
	remote::serializeWindowLayers(blob, _id, layers);

	/* Dedup: the dispatcher recomputes layers on every input commit. Compare serialized bytes, not
	structs: WindowLayer has uninitialized padding. */
	if (_lastLayersBlob.size() == blob.size()
			&& (blob.empty() || sprt::memcmp(_lastLayersBlob.data(), blob.data(), blob.size()) == 0)) {
		return;
	}
	_lastLayersBlob = blob;

	slog().info("RemoteWindow", "updateLayers: forwarding ", layers.size(), " layer(s)");
	conn->sendMessage(remote::Domain::Window, toInt(remote::WindowCode::UpdateLayers),
			BytesView(blob.data(), blob.size()));
}

void RemoteWindow::setSceneInfo(Rc<WindowSceneInfo> &&s) {
	if (_sceneInfo) {
		_sceneInfo->setChannel(nullptr);
	}
	_sceneInfo = sp::move(s);
	if (_sceneInfo) {
		// The handle's window: a client's windows are RemoteWindows, so getWindow() stays null and
		// getChannel() is what answers.
		_sceneInfo->setChannel(this);
	}
}

} // namespace stappler::xenolith
