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

#ifndef XENOLITH_FONT_XLREMOTEFONTSERVERENDPOINT_H_
#define XENOLITH_FONT_XLREMOTEFONTSERVERENDPOINT_H_

#include "XLRemoteFontServer.h" // the stappler::xenolith::RemoteFontServer interface
#include "XLFontControllerLocal.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

class FontComponent;

// Concrete server font endpoint (see RemoteFontServer). Owns the dedicated network-serving
// FontControllerLocal (its own FontLibrary + atlas, distinct from the server's local-scene
// controller), the persistent font store keyed by content hash, and the depId -> DependencyEvent
// registry. Drives the shared FontComponent GPU queue to rasterize the glyphs a client requests
// into its own atlas.
class SP_PUBLIC RemoteFontServerEndpoint : public RemoteFontServer {
public:
	// SharedModule factory. Returns the abstract interface so the server can hold it without a font
	// dep. `store` is the server's shared font store, opaque to it: null on the first call, which
	// creates the store and returns it there for the next endpoints.
	static Rc<RemoteFontServer> createServerFontEndpoint(AppThread *thread, FontComponent *,
			Rc<Ref> &store);

	virtual ~RemoteFontServerEndpoint();

	bool init(AppThread *thread, FontComponent *, Rc<Ref> &store);

	virtual void setPeer(RemotePeer *) override;
	virtual bool dispatch(uint8_t code, uint32_t serial, BytesView payload) override;
	virtual void receiveFontData(uint64_t contentHash, BytesView bytes) override;
	virtual Rc<core::DependencyEvent> reconcileDependency(uint32_t depId) override;
	virtual uint64_t pinAtlasImage() override;
	virtual Rc<core::DynamicImageInstance> resolveAtlasInstance(uint64_t imageId) override;
	virtual void reset() override;
	virtual void invalidate() override;

protected:
	// Fonts by content hash, persistent and shared by every endpoint of the server.
	struct FontStore : public Ref {
		Map<uint64_t, Rc<FontFaceData>> fonts;
	};

	void preloadDefaultFonts();
	void handleSourcesAnnounce(uint32_t serial, BytesView payload);
	void handleGlyphRequest(BytesView payload);
	Rc<core::DependencyEvent> getOrCreateDep(uint32_t depId);

	// Every batch is answered, even one that could not be drawn: the client holds its labels' frames
	// on the batch until then. App thread only.
	void sendAtlasReady(uint32_t depId, bool ok);

	// A server thread outlives its endpoints, so work completing on other threads hops through it
	// and checks `_peer` there.
	AppThread *_thread = nullptr;
	RemotePeer *_peer = nullptr;
	// Bumped on every setPeer/reset, so an answer computed for one session never reaches the next.
	uint64_t _peerGeneration = 0;
	FontComponent *_component = nullptr;
	Rc<FontLibrary> _library; // dedicated network library: isolated FaceId space
	Rc<FontControllerLocal> _controller; // network atlas owner (uses _library)
	Rc<FontStore> _store;
	Map<uint32_t, Rc<core::DependencyEvent>>
			_deps; // depId -> server-local gating event (per-connection)
	Set<uint64_t> _unknownFonts; // content hashes already reported as missing
	uint64_t _atlasStableId = 0; // constant wire id pinned to the current atlas image
};

} // namespace stappler::xenolith::font

#endif /* XENOLITH_FONT_XLREMOTEFONTSERVERENDPOINT_H_ */
