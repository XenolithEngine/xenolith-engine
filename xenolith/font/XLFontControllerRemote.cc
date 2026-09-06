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

#include "XLFontControllerRemote.h"
#include "XLFontComponent.h"
#include "XLFontRemoteWire.h"

#include "XLAppThread.h"
#include "XLClientAppThread.h" // getSharedObjects() for the mirror image
#include "XLRemoteObject.h" // remote::ObjectFactory::makeImage / registerImageData
#include "XLTexture.h"
#include "XLCoreAttachment.h"
#include "XLCoreDynamicImage.h"
#include "XLRemoteProtocol.h"
#include "SPFilesystem.h"
#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

// SourcesAnnounce reply (SourcesReady / error) deadline.
static constexpr uint64_t kSourcesAnnounceTimeoutUs = 10'000'000; // 10s

Rc<FontController> FontControllerRemote::createRemoteController(AppThread *owner) {
	return Rc<FontControllerRemote>::create(owner);
}

FontControllerRemote::~FontControllerRemote() { invalidate(nullptr); }

bool FontControllerRemote::init(AppThread *owner) {
	_owner = owner;
	_name = "RemoteFontController";
	_ownLibrary = Rc<FontLibrary>::alloc();
	_library = _ownLibrary.get();
	loadSources();
	return true;
}

void FontControllerRemote::initialize(AppThread *app) {
	// Placeholder mirror atlas. A real client mirrors the server atlas image's gAPI id (announced in
	// SourcesReady) so Label materials hash to the server's font material; for now this is a local
	// placeholder image/texture, never GPU-compiled on the client (the client has no gl Loop).
	_image = FontComponent::makeInitialImage(_name);
	_texture = Rc<Texture>::create(_image);
	// The announce is sent from update() once the connection is up (it is not yet established here).
}

void FontControllerRemote::update(AppThread *app, const UpdateTime &clock, bool wakeup) {
	if (!_announced) {
		// Retry until the connection is up and the announce actually goes out.
		_announced = sendSourcesAnnounce();
	}
	FontController::update(app, clock, wakeup);
}

void FontControllerRemote::invalidate(AppThread *) {
	if (_image) {
		_image->finalize();
		_image = nullptr;
	}
}

void FontControllerRemote::loadSources() {
	// Load the default resource fonts into our own headless library so getLayout() can answer metrics
	// queries locally. The client always has the resource fonts embedded, so this needs no transfer.
	auto builder = FontComponent::makeDefaultControllerBuilder(_name);

	for (auto &it : builder.getDataQueries()) {
		auto sourcePtr = &it.second;
		sourcePtr->data = _ownLibrary->openFontData(it.first, sourcePtr->params,
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
	}

	for (auto &it : builder.getFamilyQueries()) {
		Vector<Rc<FontFaceData>> d;
		d.reserve(it.second.sources.size());
		for (auto &s : it.second.sources) { d.emplace_back(s->data); }
		addFont(it.second.family, sp::move(d), it.second.addInFront);
	}

	setAliases(Map<String, String>(builder.getAliases()));
}

bool FontControllerRemote::sendSourcesAnnounce() {
	if (!_owner) {
		return false;
	}

	// Build the announce from the loaded source state: families -> source names, the unique sources with
	// their content hash (so the server can skip the ones it already holds), and aliases.
	Value sources;
	Value families;
	Set<const FontFaceData *> seen;
	for (auto &fam : _families) {
		Value f;
		f.setString(fam.first, "family");
		Value srcNames;
		for (auto &d : fam.second.data) {
			srcNames.addString(d->getName());
			if (seen.insert(d.get()).second) {
				Value s;
				s.setString(d->getName(), "name");
				s.setInteger(int64_t(d->getContentHash()), "hash");
				sources.addValue(sp::move(s));
			}
		}
		f.setValue(sp::move(srcNames), "sources");
		families.addValue(sp::move(f));
	}

	Value aliases;
	for (auto &a : _aliases) { aliases.setString(a.second, a.first); }

	Value announce;
	announce.setValue(sp::move(sources), "sources");
	announce.setValue(sp::move(families), "families");
	announce.setValue(sp::move(aliases), "aliases");

	bool sent = _owner->remoteSendCborWithReply(remote::Domain::Font,
			toInt(remote::FontCode::SourcesAnnounce), announce,
			[this](const remote::MessageHeader &h, BytesView payload) {
		if (remote::isError(h)) {
			log::source().warn("FontControllerRemote", "SourcesAnnounce rejected by server");
			return;
		}
		handleSourcesReady(payload);
	}, kSourcesAnnounceTimeoutUs);
	if (sent) {
		log::source().info("FontControllerRemote", "SourcesAnnounce sent");
	}
	return sent;
}

void FontControllerRemote::handleSourcesReady(BytesView payload) {
	auto v = data::read<Interface>(payload);
	_atlasImageServerId = uint64_t(v.getInteger("atlas"));

	// Hashes the server lacks must be shipped (small -> FontInline, large -> Domain::Data Font) before we
	// flip loaded. For the resource-font scenario the server already holds them, so this is empty.
	auto &missing = v.getValue("missing");
	if (missing.isArray() && !missing.empty()) {
		// TODO(e2e): ship the missing font bytes before flipping loaded.
		log::source().info("FontControllerRemote", "server is missing ", missing.size(),
				" font(s); inline / block transfer not wired yet");
	}

	// Build the mirror Texture: a static ImageData whose ImageObject carries the server's atlas wire id, so
	// a Label's MaterialInfo.images[0] == that id and hashes to the server's font material. The DataAtlas
	// itself never crosses the wire -- only the matching id is needed.
	if (auto factory = static_cast<ClientAppThread *>(_owner)->getSharedObjects()) {
		if (_atlasImageServerId) {
			_mirrorData.format = core::ImageFormat::R8_UNORM;
			_mirrorData.imageType = core::ImageType::Image2D;
			_mirrorData.extent =
					Extent3(2, 2, 1); // placeholder; extent is not part of the material hash
			_mirrorData.usage = core::ImageUsage::Sampled;
			_mirrorData.type = core::PassType::Graphics;
			_mirrorData.image =
					Rc<core::ImageObject>(factory->makeImage(_atlasImageServerId, _mirrorData));
			factory->registerImageData(_atlasImageServerId, &_mirrorData);
			_texture = Rc<Texture>::create(&_mirrorData);
			log::source().info("FontControllerRemote", "mirror texture built: image index ",
					_texture->getIndex());
		}
	}

	log::source().info("FontControllerRemote", "SourcesReady: server atlas image id ",
			_atlasImageServerId, "; controller loaded");
	setLoaded(true);
}

bool FontControllerRemote::dispatchFontMessage(uint8_t code, uint32_t serial, BytesView payload) {
	switch (remote::FontCode(code)) {
	case remote::FontCode::AtlasReady:
		// Gating is enforced server-side (the frame waits on the reconciled dependency there); the client
		// just consumes the notification.
		return true;
	default:
		log::source().warn("FontControllerRemote", "unhandled font message (code ", uint32_t(code),
				")");
		return true;
	}
}

void FontControllerRemote::submitGlyphs(AppThread *app, Vector<FontUpdateRequest> &&objects,
		Rc<core::DependencyEvent> &&dep) {
	if (!_owner) {
		return;
	}

	// GlyphRequest: the gating dependency id + one entry per face carrying (contentHash, spec, the
	// client-minted FaceId, chars). The server resolves the font by hash, opens the face with the forced
	// id, rasterizes the chars, and gates the dependency.
	Vector<GlyphRequestFace> faces;
	faces.reserve(objects.size());
	for (auto &it : objects) {
		if (!it.object) {
			continue;
		}
		auto &data = it.object->getData();
		GlyphRequestFace face;
		face.contentHash = data ? data->getContentHash() : 0;
		face.spec = it.object->getSpec();
		face.faceId = it.object->getId();
		face.chars.reserve(it.chars.size());
		for (auto c : it.chars) { face.chars.emplace_back(c); }
		faces.emplace_back(sp::move(face));
	}

	Bytes req;
	encodeGlyphRequest(req, dep ? dep->getId() : 0, faces);

	//log::source().info("FontControllerRemote", "submitGlyphs: ", objects.size(), " face(s), dep ",
	//		dep ? dep->getId() : 0);
	_owner->remoteSendRaw(remote::Domain::Font, toInt(remote::FontCode::GlyphRequest),
			BytesView(req.data(), req.size()));
}

Rc<core::DependencyEvent> FontControllerRemote::makeDependency() {
	// Empty queue-set: this event never signals on the client (no font queue here). Its id (in the client
	// half of the id space) is what the server reconciles to its real signalling event.
	return Rc<core::DependencyEvent>::alloc(core::DependencyEvent::QueueSet{},
			"FontControllerRemote");
}

void FontControllerRemote::applyBuilder(AppThread *, Builder &&) {
	log::source().warn("FontControllerRemote",
			"extend() is not supported on the remote controller yet");
}

} // namespace stappler::xenolith::font
