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

#ifndef XENOLITH_FONT_XLFONTCONTROLLERLOCAL_H_
#define XENOLITH_FONT_XLFONTCONTROLLERLOCAL_H_

#include "XLFontController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

class FontComponent;
class FontGapi;

// Local (single-process / server) FontController leaf: rasterizes glyphs and owns the GPU atlas image
// through a FontComponent gAPI endpoint. The abstract base owns only positioning + source state; this
// leaf adds the GPU touchpoints. The remote/headless counterpart is FontControllerRemote (a sibling
// leaf that never references this class).
class SP_PUBLIC FontControllerLocal : public FontController {
public:
	virtual ~FontControllerLocal();

	// `lib` defaults to the FontComponent's shared library; pass a separate library to isolate this
	// controller's FaceId space (used by the server's dedicated network-serving controller, so its
	// ids never collide with the local-scene controller's ids).
	bool init(FontComponent *, StringView name, FontLibrary *lib = nullptr);

	virtual void initialize(AppThread *) override;
	virtual void invalidate(AppThread *) override;

	virtual const Rc<core::DynamicImage> &getImage() const override { return _image; }
	virtual const Rc<Texture> &getTexture() const override { return _texture; }

protected:
	void setImage(Rc<core::DynamicImage> &&);

	virtual void submitGlyphs(AppThread *, Vector<FontUpdateRequest> &&,
			Rc<core::DependencyEvent> &&) override;
	virtual Rc<core::DependencyEvent> makeDependency() override;
	virtual void applyBuilder(AppThread *app, Builder &&) override;

	Rc<Texture> _texture;
	Rc<core::DynamicImage> _image;
	Rc<FontComponent> _component;

	// gAPI endpoint for the GPU touchpoints (atlas compile + glyph raster): the FontComponent (kept
	// alive by _component; may be shared between several local controllers).
	FontGapi *_gapi = nullptr;
};

} // namespace stappler::xenolith::font

#endif /* XENOLITH_FONT_XLFONTCONTROLLERLOCAL_H_ */
