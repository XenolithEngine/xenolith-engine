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

#ifndef XENOLITH_FONT_XLFONTCONTROLLERREMOTE_H_
#define XENOLITH_FONT_XLFONTCONTROLLERREMOTE_H_

#include "XLFontController.h"
#include "XLCoreInfo.h" // core::ImageData for the mirror atlas reference

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

// FontLibrary (stappler::font) is visible via the `using namespace stappler::font` in XLFontConfig.h.

// Headless, client-side FontController leaf. It does glyph positioning locally (its own FontLibrary, for
// metrics only -- no bitmaps) and forwards rasterization to the GPU server over remote::Domain::Font: it
// announces its font sources (content-hashed so the server skips fonts it already holds), then ships
// GlyphRequests carrying the client-minted FaceIds the server must adopt. It owns no GPU resources beyond
// a mirror atlas Texture that Labels reference -- the real atlas lives on the server.
//
// Fully independent of FontControllerLocal: neither references the other. Lives in xenolith_font (it
// needs FontLibrary), reached from xenolith_application via the createRemoteController SharedModule
// symbol, mirroring how the server reaches FontComponent::createDefaultController.
class SP_PUBLIC FontControllerRemote : public FontController {
public:
	// SharedModule factory. Returns the controller as the abstract base so the application can register
	// it via addExtension under font::FontController.
	static Rc<FontController> createRemoteController(AppThread *owner);

	virtual ~FontControllerRemote();

	bool init(AppThread *owner);

	virtual void initialize(AppThread *) override;
	virtual void invalidate(AppThread *) override;

	// Sends the SourcesAnnounce on the first tick once the connection is up (it is not yet established
	// when initialize() runs), then defers to the base update().
	virtual void update(AppThread *, const UpdateTime &clock, bool) override;

	virtual const Rc<core::DynamicImage> &getImage() const override { return _image; }
	virtual const Rc<Texture> &getTexture() const override { return _texture; }

	// remote::Domain::Font notifications addressed at the client controller (AtlasReady, ...).
	virtual bool dispatchFontMessage(uint8_t code, uint32_t serial, BytesView payload) override;

protected:
	// Serialize the glyph-raster batch (+ gating dependency id) and ship it to the server.
	virtual void submitGlyphs(AppThread *, Vector<FontUpdateRequest> &&,
			Rc<core::DependencyEvent> &&) override;
	// Client-minted dependency with an empty queue-set: it never signals locally (the client has no font
	// queue); its id travels to the server, which reconciles it to the real, frame-gating event.
	virtual Rc<core::DependencyEvent> makeDependency() override;
	virtual void applyBuilder(AppThread *app, Builder &&) override;

	void loadSources();
	bool sendSourcesAnnounce(); // returns true once it was actually sent (connection up)
	void handleSourcesReady(BytesView payload);

	AppThread *_owner = nullptr;
	bool _announced = false;
	Rc<FontLibrary> _ownLibrary; // headless metrics library; base _library points at it
	// Static mirror of the server atlas image: a thin ImageData whose ImageObject carries the server's
	// wire id, so a Label's MaterialInfo hashes to the server font material. Declared before _texture so
	// it outlives the Texture that points at it.
	core::ImageData _mirrorData;
	Rc<Texture> _texture;
	Rc<core::DynamicImage> _image;
	uint64_t _atlasImageServerId = 0;
};

} // namespace stappler::xenolith::font

#endif /* XENOLITH_FONT_XLFONTCONTROLLERREMOTE_H_ */
