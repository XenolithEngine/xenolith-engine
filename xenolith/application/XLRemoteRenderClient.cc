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
#include "XLRemoteSession.h"
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

// A client must answer AcquireFrame within this budget; the presentation engine is waiting, so a
// client that misses it is treated as gone (the watchdog fails the waiter, the connection drops).
static constexpr uint64_t kAcquireFrameReplyTimeoutUs = 2'000'000; // 2s

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

RemoteRenderClient::~RemoteRenderClient() = default;

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool RemoteRenderClient::init(NotNull<ServerAppThread> host, NotNull<RemoteSession> session) {
	_host = host;
	_session = session;
	return true;
}

bool RemoteRenderClient::isClosed() { return !_session || _session->isClosed(); }

void RemoteRenderClient::detach() {
	_session = nullptr;
	_pendingFrames.clear();
	_drawStats.clear();
}

remote::ServerConnection *RemoteRenderClient::getConnection() const {
	return _session ? _session->getConnection() : nullptr;
}

void RemoteRenderClient::announce(NotNull<remote::ObjectRegistry> registry) {
	if (isClosed()) {
		return;
	}

	Value data;
	auto &windows = data.emplace("windows");
	for (auto &it : registry->getWindows()) {
		if (!registry->isWindowVisible(it.first, _session->getId())) {
			continue;
		}
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
				// The queue's API, so the client can select a queue it can drive (see
				// Scene2d::selectServerQueue).
				v.addInteger(toInt(q->queue->getApi()));
				v.addInteger(q->queue->getTypeTag());
				v.addInteger(toInt(q->queue->getDamageFlags()));
			}
		}

		// [7] WindowInfo, always emitted so later indexes are stable; the reader detects absence by
		// the value type.
		if (auto info = it.second.window->getInfo()) {
			v.addValue(remote::serializeWindowInfo(*info));
		} else {
			v.addValue(Value());
		}

		// [8] Geometry at connect time, so the client's mirror is valid before the window first
		// moves.
		v.addValue(remote::serializeWindowGeometry(it.second.window->getWindowGeometry()));

		// [9] The CreateWindow request this window answers, for the session that sent it: the
		// client's own name for the window before the server renamed it, and what lets it bind the
		// scene it built the request with. 0 for every window nobody asked for.
		v.addInteger(it.second.creatorSession == _session->getId()
						? int64_t(it.second.creatorSerial)
						: 0);
	}

	getConnection()->sendCborMessage(remote::Domain::Global,
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

	/* [3] Frame timing, [4] the last DrawStat, appended to the per-frame message. getFrameTiming()
	is a synchronous getter, so the remote client needs a pushed mirror. Both describe the previous
	frame, as on the local path (Director::pushDrawStat is asynchronous). */
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

	// Hold the completion across the async reply and fire it exactly once (on reply or send
	// failure).
	struct FrameReply : Ref {
		Function<void(bool)> cb;
	};
	auto pending = Rc<FrameReply>::alloc();
	pending->cb = sp::move(cb);

	// The server always wraps its real FrameRequest in a LocalFrameRequestProxy; keep it alive to
	// route streamed input until the frame commits.
	auto localProxy = Rc<core::LocalFrameRequestProxy>(
			static_cast<core::LocalFrameRequestProxy *>(proxy.get()));

	auto sent = _session->sendMessageWithReply(remote::Domain::Window,
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

/* Feeds the inputs a remote client can not produce. `FrameCapture` is server state (armed on the
 * AppWindow), and the client never ships it, but a declared input that is never fed wedges the
 * frame and stops presentation. So an input is submitted every frame, empty when nothing is armed.
 * The capture is taken, not read, and `windowId` must come from the frame, not the latest window,
 * or the capture goes to the wrong window.
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
		// Empty is the normal case: nothing armed. It still has to be submitted (see above).
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

	// Resolve every target attachment and deserialize the payload once (all keys accept the same
	// input type). Client-minted gating dependency ids go into `remoteWaitDependencyIds` via
	// makeInputData; this local must outlive the deserialize call.
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
	// Report which step failed and for which attachment: unknown key, no input type, or a rejected
	// payload.
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

	// Reconcile the frame's remote dependency ids to server-local DependencyEvents that gate it
	// (font atlas update or forwarded material compile). Unknown ids are skipped.
	for (auto depId : remoteWaitDependencyIds) {
		if (auto dep = reconcileDependency(depId)) {
			input->waitDependencies.emplace_back(sp::move(dep));
		}
	}

	// Submit on the gapi loop thread (where the frame queue runs); the one input object is shared
	// across all its attachments.
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
	if (auto fs = _session ? _session->getFontServer() : nullptr) {
		return fs->reconcileDependency(depId);
	}
	return nullptr;
}

void RemoteRenderClient::handleCompileMaterials(BytesView payload) {
	auto v = data::read<Interface>(payload);
	auto windowId = uint64_t(v.getInteger("window"));

	auto reg = _host->getSharedObjects();
	auto fontServer = _session ? _session->getFontServer() : nullptr;
	if (!reg || !_session) {
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

	// Resolve a material pipeline by key as FrameContext::readMaterials does: material attachment's
	// texture-set layout -> binding pipeline layouts -> families -> graphic pipelines. The
	// top-level table is empty for a dynamically-built queue.
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

	// Resolve a static (non-atlas) image by its gAPI object id through an existing material in this
	// set that references the same ImageObject; the wire carries only the id.
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

			// The codec carries only the binding and view info. The font atlas image is rebuilt
			// from the font server's current instance; any other image is a static resource image
			// reused from an existing material in this set.
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

	// Server-local gating events signalled by the compile, registered for handleFrameInput. A fired
	// dependency is removed, so a later frame referencing its id treats it as satisfied.
	Vector<Rc<core::DependencyEvent>> events;
	for (auto &dn : v.getValue("deps").asArray()) {
		auto depId = uint32_t(dn.getInteger());
		auto ev = Rc<core::DependencyEvent>::alloc(
				core::DependencyEvent::QueueSet{Rc<core::Queue>(att->getCompiler())},
				"RemoteMaterialDep");
		// Drop the dependency from _materialDeps once signalled. The signal fires on the GPU loop
		// thread and may outlive the connection, so capture an Rc and hop to the app thread.
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
	// Not forwarded: the client selects the queue itself and reports it with AttachQueue.
}

void RemoteRenderClient::handleConstraintsChanged(const core::FrameConstraints &) {
	// Not forwarded: constraints reach the client in every AcquireFrame.
}

void RemoteRenderClient::handleWindowGeometryChanged(uint64_t windowId,
		const sprt::window::WindowGeometry &g) {
	// Forwarded: geometry has no other carrier, and AppWindow::notifyWindowGeometry calls this only
	// on a real change. A window that is not shared reports windowId 0 and is dropped.
	if (isClosed() || windowId == 0) {
		return;
	}

	Value msg;
	msg.addInteger(int64_t(windowId));
	msg.addValue(remote::serializeWindowGeometry(g));

	getConnection()->sendCborMessage(remote::Domain::Window,
			toInt(remote::WindowCode::WindowGeometryChanged), msg);
}

void RemoteRenderClient::handleInputEvents(uint64_t windowId,
		Vector<core::InputEventData> &&events) {
	// The server's window dispatches platform input here while a remote client is attached. Forward
	// the batch in the typed wire format (see serializeInputEvents).
	if (isClosed() || windowId == 0 || events.empty()) {
		return;
	}

	Bytes blob;
	remote::serializeInputEvents(blob, windowId, events);

	getConnection()->sendMessage(remote::Domain::Window, toInt(remote::WindowCode::InputEvents),
			BytesView(blob.data(), blob.size()));
}

void RemoteRenderClient::handleTextInput(uint64_t windowId, const core::TextInputState &state) {
	// Echo of the text input state decided by the window's processor; the client's only source for
	// it.
	if (isClosed() || windowId == 0) {
		return;
	}

	Value msg;
	msg.addInteger(int64_t(windowId));
	msg.addValue(remote::serializeTextInputState(state));

	getConnection()->sendCborMessage(remote::Domain::Window,
			toInt(remote::WindowCode::TextInputState), msg);
}
void RemoteRenderClient::handleFramePresented(uint64_t) {
	// Not forwarded: nothing on the receiving side uses it. A presented-frame signal, when needed,
	// is available as FrameTimingInfo::lastFrameOrder in the AcquireFrame piggyback.
}

void RemoteRenderClient::pushDrawStat(uint64_t windowId, const core::DrawStat &stat) {
	// Called on the render thread, where the connection must not be touched: hop to the app thread
	// and hold the value for the next frame request. Captures an Rc, since the push can outlive the
	// connection.
	if (!_host) {
		return;
	}
	_host->performOnAppThread([self = Rc<RemoteRenderClient>(this), windowId, stat] {
		self->_drawStats.insert_or_assign(windowId, WindowDrawStat{stat, true});
	}, this);
}

void RemoteRenderClient::handleMaterialsUpdated(uint64_t queue, NotNull<core::MaterialSet> set,
		NotNull<remote::ObjectRegistry> registry) {
	if (isClosed()) {
		return;
	}

	auto data = remote::QueueCodec::encodeMaterials(queue, *set, *registry);
	if (data.empty()) {
		return;
	}

	getConnection()->sendMessage(remote::Domain::Window, toInt(remote::WindowCode::UpdateMaterials),
			data);
}

} // namespace stappler::xenolith
