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

// Per-request reply deadlines (relative us). CompileQueue makes the server compile a render graph
// (shaders, pipelines) so it is generous; AttachQueue is a trivial readiness ack so it is short. If a
// reply does not arrive in time the request watchdog fails the waiter and the client disconnects.
static constexpr uint64_t kCompileQueueReplyTimeoutUs = 15'000'000; // 15s
static constexpr uint64_t kAttachQueueReplyTimeoutUs = 5'000'000; // 5s

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
	}, kCompileQueueReplyTimeoutUs)) {
		cb(false);
	}
}

void RemoteWindow::acquireFrame(uint64_t frameId, const core::FrameConstraints &c,
		Function<void(uint64_t queueId)> &&reply) {
	// `_client` is this window's local Director (set via RenderServerChannel::setRenderClient).
	if (!_client) {
		slog().error("RemoteWindow", "acquireFrame: no client");
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
			// Flush this frame's pending glyph requests BEFORE its FrameInput, so the server registers the
			// gating dependency (via GlyphRequest) before it reconciles the frame against it -- otherwise
			// the reconcile finds nothing and the glyphs are not actually gated. The font dependency baked
			// into the serialized `bytes` matches the one the flush submits.
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

	// Director::acquireFrame sets selectQueue() synchronously and fires this callback before its async
	// render/commit, so the selected queue is already populated here. Per-frame input is deferred.
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
		slog().debug("RemoteWindow", "acquireFrame: queue: ", id);
		reply(id);
	});
}

void RemoteWindow::compileResource(Rc<core::Resource> &&, Function<void(bool)> &&, bool preload) { }

void RemoteWindow::compileMaterials(Rc<core::MaterialInputData> &&req,
		const Vector<Rc<core::DependencyEvent>> &deps) {
	// The headless client cannot compile a runtime material (no GPU). Forward the request to the server,
	// which resolves the image refs (the atlas image id -> its DynamicImage), compiles into the window's
	// MaterialSet under the client-assigned ids, signals the gating deps, and pushes the set back.
	auto conn = _thread->getConnection();
	if (!conn || !req) {
		return;
	}

	slog().info("RemoteWindow", "compileMaterials: forwarding ",
			req->materialsToAddOrUpdate.size(), " material(s), ", deps.size(), " dep(s)");

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

	// dynamicMaterialsToUpdate / materialsToRemove are not forwarded yet (the font add-path is enough for
	// remote text; atlas growth is tracked server-side via the dynamic material).
	conn->sendCborMessage(remote::Domain::Window, toInt(remote::WindowCode::CompileMaterials), msg);
}

void RemoteWindow::compileImage(const Rc<core::DynamicImage> &, Function<void(bool)> &&) { }

void RemoteWindow::attachRenderQueue(const Rc<core::Queue> &) {
	// The Director just made the (already compiled) shared queue its active render graph -- the client is
	// now ready to serve frames for this window. Notify the server with an AttachQueue sync: only on this
	// message does it route the window's frames to us, so AcquireFrame requests can't arrive before we
	// are ready. The server replies with an empty atomic acknowledgement.
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
			slog().error("RemoteWindow", "attachRenderQueue: server rejected (code ", int(h.code), ")");
			return;
		}
		slog().debug("RemoteWindow", "attachRenderQueue: server switched to client");
	}, kAttachQueueReplyTimeoutUs)) {
		slog().error("RemoteWindow", "attachRenderQueue: failed to send ready signal");
	}
}

void RemoteWindow::setReadyForNextFrame() {
	// The client's Director has active actions/input (see Director::hasActiveInteractions) and wants
	// another frame. Forward the request so the server's PresentationEngine schedules the next frame and
	// keeps continuous progress going; fire-and-forget (no reply), same cadence as per-frame input.
	if (auto conn = _thread->getConnection()) {
		conn->sendCborMessage(remote::Domain::Window,
				toInt(remote::WindowCode::ReadyForNextFrame), Value(_id));
	}
}
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
		Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) {
	// The client is headless and cannot render locally. Forward the request to the server, which owns
	// the real window/GPU; the captured pixels return asynchronously as a Domain::Data Screenshot
	// transfer (see ClientAppThread::loadExtensions -> deliverScreenshot), matched back to `cb` by the
	// RequestScreenshot serial echoed in the transfer's announce reason. Fire-and-forget (no reply).
	auto conn = _thread ? _thread->getConnection() : nullptr;
	if (!conn) {
		slog().error("RemoteWindow", "captureScreenshot: not connected");
		if (cb) {
			cb(core::ImageInfoData(), BytesView());
		}
		return;
	}

	uint32_t serial = 0;
	if (conn->sendCborMessage(remote::Domain::Window,
				toInt(remote::WindowCode::RequestScreenshot), Value(_id), &serial)
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
	slog().debug("RemoteWindow", "deliverScreenshot: ", pixels.size(), " bytes for serial ", serial);
	if (cb) {
		cb(info, pixels);
	}
	return true;
}

bool RemoteWindow::openWindowMenu(Vec2 pos) { return false; }

void RemoteWindow::handleInputEvents(Vector<core::InputEventData> &&events) {
	// Server-forwarded platform input (WindowCode::InputEvents): replay it into the local Director's
	// render endpoint (_client), exactly as a real window would feed its own scene. Runs on the app
	// thread (the connection dispatch loop), where the scene graph lives. Window-state events also
	// update the mirrored state so getWindowState() stays consistent.
	for (auto &event : events) {
		if (event.event == core::InputEventName::WindowState) {
			_state = event.window.state;
		}
	}
	if (_client) {
		_client->handleInputEvents(sp::move(events));
	}
}

void RemoteWindow::updateLayers(sprt::window::Vector<sprt::window::WindowLayer> &&layers) {
	// The client's scene graph (InputDispatcher) computes the window's interaction layers (hit/cursor/
	// drag regions), but the server owns the real OS window. Forward them as a raw blob
	// [u64 windowId (network order)][WindowLayer[] native layout]; the server applies them to its native
	// window. WindowLayer is trivially copyable, so the array ships verbatim (one build/ABI).
	static_assert(__is_trivially_copyable(sprt::window::WindowLayer),
			"WindowLayer must be trivially copyable to ship as a raw blob");
	auto conn = _thread ? _thread->getConnection() : nullptr;
	if (!conn) {
		return;
	}

	const size_t payloadBytes = layers.size() * sizeof(sprt::window::WindowLayer);
	const uint8_t *payloadPtr = reinterpret_cast<const uint8_t *>(layers.data());

	// Dedup: the dispatcher recomputes the layer set every input commit; skip an unchanged set so the
	// server is not flooded with identical updates each frame.
	if (_lastLayersBlob.size() == payloadBytes
			&& (payloadBytes == 0
					|| sprt::memcmp(_lastLayersBlob.data(), payloadPtr, payloadBytes) == 0)) {
		return;
	}
	_lastLayersBlob.assign(payloadPtr, payloadPtr + payloadBytes);

	Bytes blob;
	blob.resize(sizeof(uint64_t) + payloadBytes);
	uint64_t widN = sprt::byteorder::HostToNetwork(_id);
	__sprt_memcpy(blob.data(), &widN, sizeof(uint64_t));
	if (payloadBytes) {
		__sprt_memcpy(blob.data() + sizeof(uint64_t), payloadPtr, payloadBytes);
	}

	slog().info("RemoteWindow", "updateLayers: forwarding ", layers.size(), " layer(s)");
	conn->sendMessage(remote::Domain::Window, toInt(remote::WindowCode::UpdateLayers),
			BytesView(blob.data(), blob.size()));
}

} // namespace stappler::xenolith
