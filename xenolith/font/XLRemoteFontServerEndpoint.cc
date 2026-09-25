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

#include "XLRemoteFontServerEndpoint.h"
#include "XLFontComponent.h"
#include "XLFontRemoteWire.h"

#include "XLAppThread.h"
#include "XLServerAppThread.h" // getSharedObjects() for the atlas wire-id
#include "XLRemoteObject.h" // remote::ObjectRegistry::share
#include "XLCoreAttachment.h"
#include "XLCoreDynamicImage.h"
#include "XLRemoteProtocol.h"
#include "SPFilesystem.h"
#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

Rc<RemoteFontServer> RemoteFontServerEndpoint::createServerFontEndpoint(AppThread *thread,
		FontComponent *comp, Rc<Ref> &store) {
	return Rc<RemoteFontServerEndpoint>::create(thread, comp, store);
}

RemoteFontServerEndpoint::~RemoteFontServerEndpoint() { }

bool RemoteFontServerEndpoint::init(AppThread *thread, FontComponent *comp, Rc<Ref> &store) {
	_thread = thread;
	_component = comp;
	// Dedicated network library + controller: a FaceId space fully isolated from the server's
	// local-scene controller and from other clients, with its own atlas (the one its client
	// references).
	_library = Rc<FontLibrary>::alloc();
	_controller =
			Rc<FontControllerLocal>::create(comp, "RemoteFontServerController", _library.get());
	if (_controller) {
		_controller->initialize(_thread);
	}
	if (store) {
		_store = static_cast<FontStore *>(store.get());
	} else {
		_store = Rc<FontStore>::alloc();
		preloadDefaultFonts();
		store = _store;
	}
	return true;
}

void RemoteFontServerEndpoint::setPeer(RemotePeer *peer) {
	_peer = peer;
	++_peerGeneration;
}

void RemoteFontServerEndpoint::preloadDefaultFonts() {
	// The server holds the default resource fonts persistently; pin them by content hash so a
	// client that announces the same fonts never has to re-send their bytes.
	auto builder = FontComponent::makeDefaultControllerBuilder("RemoteFontServer");
	for (auto &it : builder.getDataQueries()) {
		auto sourcePtr = &it.second;
		auto data = _library->openFontData(it.first, sourcePtr->params,
				sourcePtr->preconfiguredParams, [&]() -> FontLibrary::FontData {
			if (sourcePtr->fontCallback) {
				return FontLibrary::FontData(sp::move(sourcePtr->fontCallback));
			} else if (!sourcePtr->fontExternalData.empty()) {
				return FontLibrary::FontData(sourcePtr->fontExternalData, true);
			} else if (!sourcePtr->fontMemoryData.empty()) {
				return FontLibrary::FontData(sp::move(sourcePtr->fontMemoryData));
			} else if (!sourcePtr->fontFilePath.empty()) {
				auto d = filesystem::readIntoMemory<Interface>(FileInfo{sourcePtr->fontFilePath});
				if (!d.empty()) {
					return FontLibrary::FontData(sp::move(d));
				}
			}
			return FontLibrary::FontData(BytesView(), false);
		});
		if (data) {
			_store->fonts.emplace(data->getContentHash(), data);
		}
	}
}

bool RemoteFontServerEndpoint::dispatch(uint8_t code, uint32_t serial, BytesView payload) {
	switch (remote::FontCode(code)) {
	case remote::FontCode::SourcesAnnounce: handleSourcesAnnounce(serial, payload); return true;
	case remote::FontCode::GlyphRequest: handleGlyphRequest(payload); return true;
	case remote::FontCode::FontInline: {
		// A small missing font shipped inline {contentHash, bytes}.
		auto v = data::read<Interface>(payload);
		receiveFontData(uint64_t(v.getInteger("hash")), v.getBytes("bytes"));
		return true;
	}
	default:
		log::source().warn("RemoteFontServerEndpoint", "unhandled font message (code ",
				uint32_t(code), ")");
		return true;
	}
}

void RemoteFontServerEndpoint::handleSourcesAnnounce(uint32_t serial, BytesView payload) {
	if (!_peer) {
		return;
	}
	auto v = data::read<Interface>(payload);

	// Report only the content hashes we do not already hold; the client ships just those.
	Value missing;
	for (auto &s : v.getValue("sources").asArray()) {
		auto hash = uint64_t(s.getInteger("hash"));
		if (_store->fonts.find(hash) == _store->fonts.end()) {
			missing.addInteger(int64_t(hash));
		}
	}

	// Pin the network atlas image to a stable wire id and announce it, so the client can build a
	// mirror Texture with the matching index -- the identity its Label materials must carry to hash
	// to the server's font material.
	uint64_t atlasId = pinAtlasImage();
	if (atlasId == 0) {
		log::source().warn("RemoteFontServerEndpoint",
				"network atlas image not compiled yet; mirror identity unavailable");
	}

	Value reply;
	reply.setValue(sp::move(missing), "missing");
	reply.setInteger(int64_t(atlasId), "atlas");
	_peer->remoteSendCborReply(serial, remote::Domain::Font, toInt(remote::FontCode::SourcesReady),
			reply);
	log::source().info("RemoteFontServerEndpoint", "SourcesReady: atlas id ", atlasId, ", ",
			reply.getValue("missing").size(), " missing");
}

void RemoteFontServerEndpoint::handleGlyphRequest(BytesView payload) {
	if (!_peer) {
		return;
	}
	uint32_t depId = 0;
	Vector<GlyphRequestFace> faces;
	if (!decodeGlyphRequest(payload, depId, faces)) {
		log::source().warn("RemoteFontServerEndpoint", "malformed glyph request (", payload.size(),
				" bytes)");
		sendAtlasReady(depId, false);
		return;
	}
	if (!_component || !_controller) {
		sendAtlasReady(depId, false);
		return;
	}

	// A face that can not be drawn still lets the rest of the batch through, but the batch fails:
	// the client resends it on its next flush.
	bool complete = true;
	Vector<FontUpdateRequest> requests;
	for (auto &f : faces) {
		auto sit = _store->fonts.find(f.contentHash);
		if (sit == _store->fonts.end()) {
			if (_unknownFonts.emplace(f.contentHash).second) {
				log::source().warn("RemoteFontServerEndpoint",
						"glyph request for unknown font hash ", f.contentHash);
			}
			complete = false;
			continue;
		}
		// Adopt the client's FaceId so the CharIds it baked into its vertexes resolve in our atlas.
		auto face = _library->openFontFace(sit->second, f.spec, f.faceId);
		if (!face) {
			complete = false;
			continue;
		}
		requests.emplace_back(
				FontUpdateRequest{sp::move(face), sp::move(f.chars), false, _library});
	}

	auto dep = getOrCreateDep(depId);

	// Rasterize into the network controller's atlas. The same dependency object is the font frame's
	// signal dependency (FontComponent::updateImage adds it) and the render frame's wait dependency
	// (reconciled in RemoteRenderClient::handleFrameInput) -- so the frame can't render until these
	// glyphs are packed.
	_component->updateImage(_thread->getLooper(), _controller->getImage(), sp::move(requests),
			Rc<core::DependencyEvent>(dep),
			[self = Rc<RemoteFontServerEndpoint>(this), thread = Rc<AppThread>(_thread),
					generation = _peerGeneration, depId, complete](bool ok) {
		// Hop to the app thread (the connection is app-thread-only) to notify the client, if it is
		// still the one that asked.
		thread->performOnAppThread([self, generation, depId, ok, complete]() {
			if (self->_peerGeneration == generation) {
				self->sendAtlasReady(depId, ok && complete);
			}
		}, self.get());
	});
}

void RemoteFontServerEndpoint::sendAtlasReady(uint32_t depId, bool ok) {
	// Batch 0 is an ungated one: nobody waits for it.
	if (!_peer || depId == 0) {
		return;
	}
	Value r;
	r.setInteger(int64_t(depId), "dep");
	r.setInteger(ok ? 1 : 0, "ok");
	_peer->remoteSendCbor(remote::Domain::Font, toInt(remote::FontCode::AtlasReady), r);
}

Rc<core::DependencyEvent> RemoteFontServerEndpoint::getOrCreateDep(uint32_t depId) {
	auto it = _deps.find(depId);
	if (it != _deps.end()) {
		return it->second;
	}
	auto dep = Rc<core::DependencyEvent>::alloc(
			core::DependencyEvent::QueueSet{_component->getQueue()}, "RemoteFontServerDep");
	// Drop the gating dependency from _deps once the atlas update signals it, so the registry does
	// not grow unbounded (one entry per GlyphRequest, otherwise cleared only on reset). The signal
	// fires on the GPU loop thread and the event can outlive this connection, so guard `this` by
	// refcount (Rc captured now, while `this` is alive) and hop to the app thread, where _deps
	// lives. A later frame referencing a removed id finds nothing in reconcileDependency and treats
	// it as already satisfied.
	dep->setSignalCallback(
			[self = Rc<RemoteFontServerEndpoint>(this), thread = Rc<AppThread>(_thread),
					generation = _peerGeneration, depId]() {
		thread->performOnAppThread([self, generation, depId]() {
			if (self->_peerGeneration == generation) {
				self->_deps.erase(depId);
			}
		}, self.get());
	});
	_deps.emplace(depId, dep);
	return dep;
}

Rc<core::DependencyEvent> RemoteFontServerEndpoint::reconcileDependency(uint32_t depId) {
	// Lookup-only: gate a frame only on a dependency we are actually rasterizing for. An id we have
	// not seen a GlyphRequest for (e.g. a non-font dependency) is left ungated.
	auto it = _deps.find(depId);
	return it != _deps.end() ? it->second : nullptr;
}

void RemoteFontServerEndpoint::receiveFontData(uint64_t contentHash, BytesView bytes) {
	if (bytes.empty() || _store->fonts.find(contentHash) != _store->fonts.end()) {
		return;
	}
	// TODO(e2e): route through FontLibrary::openFontData so variable-font params are inspected. A
	// direct FontFaceData is enough to pin the bytes for compile-stage / the block-transfer path.
	if (auto data = Rc<FontFaceData>::create(toString("remote:", contentHash), bytes, false)) {
		_store->fonts.emplace(contentHash, data);
	}
}

uint64_t RemoteFontServerEndpoint::pinAtlasImage() {
	if (!_thread || !_controller) {
		return _atlasStableId;
	}
	auto reg = static_cast<ServerAppThread *>(_thread)->getSharedObjects();
	if (!reg) {
		return _atlasStableId;
	}
	auto inst = _controller->getImage()->getInstance();
	if (!inst || !inst->data.image) {
		return _atlasStableId; // atlas not compiled yet
	}
	if (_atlasStableId == 0) {
		// First share allocates the id; reuse it for every subsequent (replaced) atlas ImageObject.
		_atlasStableId = reg->share(inst->data.image.get());
	} else {
		reg->pinObject(inst->data.image.get(), _atlasStableId);
	}
	return _atlasStableId;
}

Rc<core::DynamicImageInstance> RemoteFontServerEndpoint::resolveAtlasInstance(uint64_t imageId) {
	if (imageId == 0 || imageId != _atlasStableId || !_controller) {
		return nullptr;
	}
	return _controller->getImage()->getInstance();
}

void RemoteFontServerEndpoint::reset() {
	// Drop per-connection gating events and the pinned atlas id. The endpoint is not reused: its
	// faces carry this client's FaceIds (see ServerAppThread::resetSession).
	_peer = nullptr;
	++_peerGeneration;
	_deps.clear();
	_atlasStableId = 0;
}

void RemoteFontServerEndpoint::invalidate() {
	reset();

	// The atlas holds a cycle through its DynamicImage instances, which is what finalize() breaks -
	// the same teardown the local-scene controller gets from AppThread::finalizeExtensions().
	// Dropping the controller without it leaves the image (and its device memory) alive past the
	// gapi device.
	if (_controller) {
		_controller->invalidate(_thread);
		_controller = nullptr;
	}

	_store = nullptr;
	_library = nullptr;
	_component = nullptr;
	_thread = nullptr;
}

} // namespace stappler::xenolith::font
