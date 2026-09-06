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
#include "XLRemoteObject.h" // shared queue / window resolution
#include "XLServerAppThread.h"
#include "XLRemoteFontServer.h" // reconcile remote font dependency ids + resolve the atlas image
#include "XLAppWindow.h" // window->compileMaterials
#include "XLCoreFrameRequestProxy.h"
#include "XLCoreAttachment.h" // complete core::Attachment for makeInputData()
#include "XLCoreMaterial.h" // reconstruct forwarded materials
#include "XLCoreDynamicImage.h" // DynamicImageInstance for the atlas material image
#include "XLCoreQueue.h" // getGraphicPipeline
#include "XLCoreLoop.h" // gapi loop performOnThread for frame-input submission
#include "XLCoreFrameCapture.h" // FrameCaptureAttachmentName + FrameCaptureInput

#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// A client must answer an AcquireFrame request within this budget. It is short by design: a frame the
// presentation engine is already waiting on must not stall, and a client that misses it is treated as
// gone (the request watchdog fails the waiter and the server drops the connection).
static constexpr uint64_t kAcquireFrameReplyTimeoutUs = 2'000'000; // 2s

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
	_drawStats.clear();
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
				// What the queue IS, so the client can select one it can actually drive instead of
				// recognising a name both sides agreed on out of band (M3.3). See
				// Scene2d::selectServerQueue.
				v.addInteger(toInt(q->queue->getApi()));
				v.addInteger(q->queue->getTypeTag());
				v.addInteger(toInt(q->queue->getDamageFlags()));
			}
		}

		// [7] WindowInfo. Emitted UNCONDITIONALLY, even when the window has none: it used to be
		// skipped, which made every later index depend on whether a window happened to have info,
		// and an index that moves is exactly the defect D4 was. The reader tells "absent" from
		// "present" by the value's TYPE, not by its position.
		if (auto info = it.second.window->getInfo()) {
			v.addValue(remote::serializeWindowInfo(*info));
		} else {
			v.addValue(Value());
		}

		// [8] Geometry as of connect time. Without it the client's mirror is zeros until the window
		// first moves -- and "0,0 0x0" is indistinguishable from a window at the top-left corner.
		v.addValue(remote::serializeWindowGeometry(it.second.window->getWindowGeometry()));
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

	/* [3] Frame timing, [4] the last DrawStat. Appended to a message that already goes out once per
	frame, rather than given messages of their own.
	
	getFrameTiming() is a SYNCHRONOUS by-value getter on the channel -- six call sites in Director
	feed the FPS overlay -- so a remote client cannot answer it with a request; it needs a mirror,
	and a mirror needs a push. This is that push, at zero extra messages.
	
	Both describe the frame BEFORE this one. That is not a compromise made for the wire: the local
	path is the same, because Director::pushDrawStat hops to the app thread asynchronously and the
	overlay reads whatever landed. */
	if (auto registry = _host->getSharedObjects()) {
		if (auto w = static_cast<AppWindow *>(registry->resolveWindow(windowId))) {
			req.addValue(remote::serializeFrameTiming(w->getFrameTiming()));
		}
	}
	// This window's own statistics, never another's.
	auto statIt = _drawStats.find(windowId);
	if (statIt != _drawStats.end() && statIt->second.dirty) {
		if (!req.hasValue(3)) {
			req.addValue(Value()); // keep [4] at index 4 even when the window went away
		}
		req.addValue(remote::serializeDrawStat(statIt->second.stat));
		statIt->second.dirty = false;
	}

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
			[this, pending, frameId, windowId, localProxy](const remote::MessageHeader &h,
					BytesView payload) {
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
		//log::source().info("RemoteRenderClient", "AcquireFrame ", frameId, " -> queue '",
		//		sq->queue->getName(), "' (id ", queueId, ")");
		localProxy->selectQueue(sq->queue);
		_pendingFrames.emplace(frameId, PendingFrame{localProxy, windowId});
		submitServerOwnedInputs(frameId, windowId, localProxy);
		pending->cb(true);
	},
			kAcquireFrameReplyTimeoutUs);

	if (!sent) {
		pending->cb(false);
	}
}

/* Feed the inputs a remote client CANNOT produce.
 *
 * `FrameCapture` carries which rectangles of this frame the window wants copied out. That is server
 * state -- it lives on the AppWindow and is armed by whoever asked for a capture on this side -- and
 * the client's RemoteWindow has no way to know it: its takeFrameCaptureInput() is the base's, which
 * answers null. So the client's frame context never ships this attachment.
 *
 * And an input attachment that is declared and then never fed does not degrade. It WEDGES: the
 * FrameRequest waits for an input that never comes, the frame never completes, it stays in
 * PresentationEngine::_activeFrames, and scheduleNextImage refuses to schedule anything after it.
 * The window stops producing frames entirely, which reads from outside as "the client connected but
 * the picture never changed". (XL2dFrameContext::submitInput carries the same warning, which is why
 * the local path submits an empty capture on EVERY frame rather than skipping it.)
 *
 * So the server supplies it here, once per frame, exactly as the local frame context does -- taken,
 * not read, because two frames must never carry the same capture.
 *
 * `windowId` comes from the FRAME, not from whatever window asked most recently. This runs inside the
 * asynchronous reply to AcquireFrame, so with more than one window the two are routinely different --
 * and because the capture is TAKEN, getting it wrong does not merely misattribute: it steals the
 * capture from the window that armed it and hands it to one that did not ask.
 */
void RemoteRenderClient::submitServerOwnedInputs(uint64_t frameId, uint64_t windowId,
		NotNull<core::LocalFrameRequestProxy> proxy) {
	auto req = Rc<core::FrameRequest>(proxy->getRequest());
	if (!req) {
		return;
	}
	auto &queue = req->getQueue();
	if (!queue) {
		return;
	}

	const core::AttachmentData *captureData = nullptr;
	for (auto &a : queue->getAttachments()) {
		if (a->key == core::FrameCaptureAttachmentName
				&& hasFlag(a->usage, core::AttachmentUsage::Input)) {
			captureData = a;
			break;
		}
	}
	if (!captureData) {
		return; // a queue without the capture attachment has nothing to feed
	}

	Rc<core::FrameCaptureInput> capture;
	if (auto reg = _host ? _host->getSharedObjects() : nullptr) {
		if (auto w = static_cast<AppWindow *>(reg->resolveWindow(windowId))) {
			capture = w->takeFrameCaptureInput();
		}
	}
	if (!capture) {
		// Empty is the normal case: nothing armed. It still has to be submitted -- see above.
		capture = Rc<core::FrameCaptureInput>::alloc();
	}

	// Inputs are added on the gapi loop thread, like every other input on this path.
	if (auto loop = _host->getGlLoop()) {
		loop->performOnThread([req, captureData, capture = sp::move(capture)]() mutable {
			req->addInput(captureData, sp::move(capture));
		});
	}
}

void RemoteRenderClient::handleFrameInput(uint64_t frameId, SpanView<StringView> attachmentKeys,
		BytesView bytes) {
	auto it = _pendingFrames.find(frameId);
	if (it == _pendingFrames.end()) {
		return;
	}
	auto req = Rc<core::FrameRequest>(it->second.proxy->getRequest());
	if (!req) {
		return;
	}
	auto &queue = req->getQueue();
	if (!queue) {
		return;
	}

	// Resolve every target attachment; deserialize the shared payload once (the first attachment mints
	// the concrete input type -- all keys in a multi-key message accept the same type). The client-minted
	// gating dependency ids carried in the blob are output into `remoteWaitDependencyIds`, wired into the
	// input via makeInputData and filled by its deserialize (this local must outlive that call).
	Vector<const core::AttachmentData *> atts;
	Rc<core::AttachmentInputData> input;
	Vector<uint32_t> remoteWaitDependencyIds;
	for (auto key : attachmentKeys) {
		auto attData = queue->getAttachment(key);
		if (!attData || !attData->attachment) {
			log::source().warn("RemoteRenderClient", "FrameInput ", frameId,
					" for unknown attachment '", key, "'");
			continue;
		}
		if (!input) {
			input = attData->attachment->makeInputData(this, it->second.windowId);
		}
		atts.emplace_back(attData);
	}
	// Say WHICH step failed and for which attachment: "failed to reconstruct" covers three
	// unrelated causes (a key this queue does not have, an attachment with no input type, a payload
	// the input rejects) and they are fixed in three different places.
	if (atts.empty() || !input) {
		StringStream keys;
		for (auto key : attachmentKeys) { keys << " '" << key << "'"; }
		log::source().warn("RemoteRenderClient", "FrameInput ", frameId,
				": no attachment of queue '", queue->getName(), "' accepts input for", keys.str());
		return;
	}
	if (!input->deserialize(bytes, &remoteWaitDependencyIds)) {
		log::source().warn("RemoteRenderClient", "FrameInput ", frameId, ": attachment '",
				atts.front()->key, "' rejected its ", bytes.size(), "-byte payload");
		return;
	}

	// Reconcile this frame's remote dependency ids to the server-local DependencyEvents that gate it: a
	// font atlas update (font server) or a forwarded material compile (_materialDeps). The frame cannot
	// render until those are signalled. Unknown ids (nothing server-side waits on them) are skipped.
	for (auto depId : remoteWaitDependencyIds) {
		if (auto dep = reconcileDependency(depId)) {
			input->waitDependencies.emplace_back(sp::move(dep));
		}
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

Rc<core::DependencyEvent> RemoteRenderClient::reconcileDependency(uint32_t depId) {
	auto it = _materialDeps.find(depId);
	if (it != _materialDeps.end()) {
		return it->second;
	}
	if (auto fs = _host->getFontServer()) {
		return fs->reconcileDependency(depId);
	}
	return nullptr;
}

void RemoteRenderClient::handleCompileMaterials(BytesView payload) {
	auto v = data::read<Interface>(payload);
	auto windowId = uint64_t(v.getInteger("window"));

	auto reg = _host->getSharedObjects();
	auto fontServer = _host->getFontServer();
	if (!reg) {
		return;
	}

	// Resolve the window's shared queue + its (single) material attachment.
	const core::MaterialAttachment *att = nullptr;
	core::Queue *queue = nullptr;
	auto &windows = reg->getWindows();
	auto wit = windows.find(windowId);
	if (wit != windows.end()) {
		for (auto qid : wit->second.queues) {
			if (auto qi = reg->resolveQueue(qid)) {
				if (!qi->materials.empty()) {
					att = qi->materials.begin()->first;
					queue = qi->queue.get();
					break;
				}
			}
		}
	}
	auto window = static_cast<AppWindow *>(reg->resolveWindow(windowId));
	if (!att || !queue || !window) {
		log::source().warn("RemoteRenderClient",
				"CompileMaterials: no material attachment / window ", windowId);
		return;
	}

	auto input = Rc<core::MaterialInputData>::alloc();
	input->setAttachment(att);

	// Resolve a material pipeline by key the same way FrameContext::readMaterials does: walk the material
	// attachment's target texture-set-layout -> binding pipeline layouts -> families -> graphic pipelines.
	// The queue's top-level graphicPipelines table is not populated for a dynamically-built render queue
	// (Queue::getGraphicPipeline returns null), but the family graph that drives material compilation is
	// intact -- this is the same graph the client used to pick the pipeline it forwarded.
	auto resolvePipeline = [&](StringView key) -> const core::GraphicPipelineData * {
		if (auto tl = att->getTargetLayout()) {
			for (auto bl : tl->bindingLayouts) {
				for (auto fam : bl->families) {
					for (auto p : fam->graphicPipelines) {
						if (p->key == key) {
							return p;
						}
					}
				}
			}
		}
		// Fallback: the top-level table (populated for statically-compiled queues).
		return queue->getGraphicPipeline(key);
	};

	// Resolve a static (non-atlas) resource image by its gAPI object id: find the real ImageData via an
	// existing material in this attachment's set that already references the same ImageObject. The server
	// owns the ImageData; the wire only carried the object id.
	auto resolveStaticImageData = [&](uint64_t imageId) -> const core::ImageData * {
		auto obj = reg->resolveObject(imageId);
		if (!obj) {
			return nullptr;
		}
		if (auto set = att->getMaterials()) {
			for (auto &mit : set->getMaterials()) {
				for (auto &img : mit.second->getImages()) {
					if (img.image && img.image->image.get() == obj) {
						return img.image;
					}
				}
			}
		}
		return nullptr;
	};

	for (auto &mn : v.getValue("mats").asArray()) {
		auto id = core::MaterialId(mn.getInteger("id"));
		auto pipeline = resolvePipeline(mn.getString("pl"));
		if (!pipeline) {
			log::source().warn("RemoteRenderClient", "CompileMaterials: unknown pipeline '",
					mn.getString("pl"), "'");
			continue;
		}
		Vector<core::MaterialImage> images;
		bool ok = true;
		for (auto &in : mn.getValue("imgs").asArray()) {
			uint64_t imageId = 0;
			auto mi = remote::deserializeMaterialImage(in, imageId);

			// The codec carries only the descriptor binding + view info; the server resolves the real
			// image by id. The font atlas is a runtime dynamic (atlas-tracked) image, rebuilt from the
			// font server's current instance; any other image is a static resource image (e.g.
			// SolidImage), reused from an existing material in this attachment's set.
			if (auto inst = fontServer ? fontServer->resolveAtlasInstance(imageId) : nullptr) {
				mi.dynamic = inst;
				mi.image = &inst->data;
			} else if (auto imgData = resolveStaticImageData(imageId)) {
				mi.image = imgData;
			} else {
				log::source().warn("RemoteRenderClient", "CompileMaterials: unresolved image id ",
						imageId);
				ok = false;
				break;
			}
			images.emplace_back(sp::move(mi));
		}
		if (!ok || images.empty()) {
			continue;
		}
		if (auto mat = Rc<core::Material>::create(id, pipeline, sp::move(images), Rc<Ref>())) {
			input->materialsToAddOrUpdate.emplace_back(sp::move(mat));
		}
	}

	if (input->materialsToAddOrUpdate.empty()) {
		return;
	}

	// Server-local gating events the compile signals; registered so handleFrameInput reconciles a frame's
	// material dependency id to them (the frame waits until the material is compiled). The registry is
	// drained by a per-event signal callback (below): once a dependency fires it is removed, so a later
	// frame referencing that id finds nothing in reconcileDependency and treats it as already satisfied.
	Vector<Rc<core::DependencyEvent>> events;
	for (auto &dn : v.getValue("deps").asArray()) {
		auto depId = uint32_t(dn.getInteger());
		auto ev = Rc<core::DependencyEvent>::alloc(
				core::DependencyEvent::QueueSet{Rc<core::Queue>(att->getCompiler())},
				"RemoteMaterialDep");
		// Drop the mirror dependency from _materialDeps once the compile signals it. The signal fires on
		// the GPU loop thread and the event can outlive this connection, so guard the client by refcount
		// (Rc captured now, while `this` is alive) and hop to the app thread, where _materialDeps lives.
		ev->setSignalCallback(
				[self = Rc<RemoteRenderClient>(this), host = Rc<ServerAppThread>(_host), depId]() {
			host->performOnAppThread([self, depId]() { self->_materialDeps.erase(depId); },
					self.get());
		});
		_materialDeps.emplace(depId, ev);
		events.emplace_back(sp::move(ev));
	}

	//log::source().info("RemoteRenderClient", "CompileMaterials: compiling ",
	//		input->materialsToAddOrUpdate.size(), " material(s) for window ", windowId);
	window->compileMaterials(sp::move(input), events);
}

void RemoteRenderClient::handleRenderQueueAttached(const Rc<core::Queue> &) {
	// Deliberately not forwarded. Its only consumer is Director::_availableQueues, which is written
	// and never read; and on this path the CLIENT is the side that picks the queue
	// (Scene2d::selectServerQueue over the announced list) and tells the server through
	// AttachQueue -- so a message here would inform the better-informed side. See M4 in the plan.
}

void RemoteRenderClient::handleConstraintsChanged(const core::FrameConstraints &) {
	// Deliberately not forwarded either, but for the opposite reason: the constraints already reach
	// the client in every AcquireFrame, where its Director applies them exactly as a local one does.
	// A second carrier would be a second source of truth. (This hook has no caller in the engine at
	// all today -- AppWindow::handleSwapchainUpdated notifies only the native window.)
}

void RemoteRenderClient::handleWindowGeometryChanged(uint64_t windowId,
		const sprt::window::WindowGeometry &g) {
	// This one IS forwarded: geometry has no other carrier, and the server has already decided the
	// change is real (AppWindow::notifyWindowGeometry compares against its mirror before calling),
	// so a window that is merely redrawing sends nothing.
	//
	// windowId is the caller's own, so this no longer has to wait for a first frame to learn which
	// window it is talking about -- and no longer misroutes to the window that happened to render
	// last. A window that is not shared reports 0 and is dropped, which is correct: the client has
	// never heard of it.
	if (isClosed() || windowId == 0) {
		return;
	}

	Value msg;
	msg.addInteger(int64_t(windowId));
	msg.addValue(remote::serializeWindowGeometry(g));

	_connection->sendCborMessage(remote::Domain::Window,
			toInt(remote::WindowCode::WindowGeometryChanged), msg);
}

void RemoteRenderClient::handleInputEvents(uint64_t windowId,
		Vector<core::InputEventData> &&events) {
	// The server's window dispatches platform input here (this client is the window's render endpoint
	// while a remote client is attached). Forward the whole batch in the typed wire format -- field
	// by field, network byte order, the variant chosen by the event (see serializeInputEvents).
	//
	// This used to be a raw dump of the struct array, which is why the ABI tag had to gate the
	// session: between builds that laid InputEventData out differently, the receiver read padding as
	// a keycode. Nothing here depends on this build's layout any more.
	if (!_connection || _connection->isClosed() || windowId == 0 || events.empty()) {
		return;
	}

	Bytes blob;
	remote::serializeInputEvents(blob, windowId, events);

	_connection->sendMessage(remote::Domain::Window, toInt(remote::WindowCode::InputEvents),
			BytesView(blob.data(), blob.size()));
}

void RemoteRenderClient::handleTextInput(uint64_t windowId, const core::TextInputState &state) {
	// The window's processor decided the state changed; the client's widget has no other way to
	// learn it. This is the echo half of text input, and the only source of truth for the field on
	// the far side.
	if (isClosed() || windowId == 0) {
		return;
	}

	Value msg;
	msg.addInteger(int64_t(windowId));
	msg.addValue(remote::serializeTextInputState(state));

	_connection->sendCborMessage(remote::Domain::Window, toInt(remote::WindowCode::TextInputState),
			msg);
}
void RemoteRenderClient::handleFramePresented(uint64_t) {
	// Not forwarded, and deliberately not made to fire either. Nothing calls this hook anywhere in
	// the engine (the one that does fire is the unrelated PresentationWindow::handleFramePresented,
	// which takes a frame rather than an order), and Director::handleFramePresented is an explicit
	// no-op on the receiving side. Wiring it would mean a per-frame thread hop plus a per-frame
	// message to move a number into something that discards it.
	//
	// When client-side pacing does need a presented-frame signal, the carrier is the AcquireFrame
	// piggyback below -- already once per frame, already on the app thread -- and the number is
	// already there as FrameTimingInfo::lastFrameOrder.
}

void RemoteRenderClient::pushDrawStat(uint64_t windowId, const core::DrawStat &stat) {
	// Arrives on the RENDER thread (the vertex pass calls it through FrameContextHandle::client),
	// where the connection must not be touched. Hop to the app thread and hold the value for the
	// next frame request, exactly as Director::pushDrawStat holds it for the local FPS overlay.
	//
	// This is also why a message of its own would buy nothing: the hop is needed either way, so a
	// separate DrawStat message would be this plus a message.
	//
	// Rc, not a raw `this`: the push can outlive the connection (resetRemoteClient drops the
	// client), and the task must then be a no-op rather than a use-after-free.
	if (!_host) {
		return;
	}
	_host->performOnAppThread([self = Rc<RemoteRenderClient>(this), windowId, stat] {
		self->_drawStats.insert_or_assign(windowId, WindowDrawStat{stat, true});
	}, this);
}

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
