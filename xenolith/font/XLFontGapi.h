/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_FONT_XLFONTGAPI_H_
#define XENOLITH_FONT_XLFONTGAPI_H_

#include "XLFontController.h" // FontUpdateRequest + core types
#include "XLCoreAttachment.h" // core::DependencyEvent

#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

//
// FontGapi: the gAPI (GPU) endpoints FontController relies on, abstracted so the same
// FontController can run on both sides of a client/server split (it operates over the same font
// data; only its GPU touchpoints differ).
//   - Server / single process: implemented by FontComponent (direct to the gl Loop / VkFontQueue).
//   - Client (remote):         FontGapiProxy forwards the calls to the server.
//
// FontController holds a non-owning FontGapi* (the local impl is its FontComponent, kept alive by
// the controller's _component Rc).
//
class SP_PUBLIC FontGapi {
public:
	virtual ~FontGapi();

	// Compile/upload the glyph atlas image on the GPU.
	virtual void compileImage(const Rc<core::DynamicImage> &, Function<void(bool)> &&) = 0;

	// Rasterize the requested glyphs into the atlas (async, on the GPU loop), signalling
	// `dependency` and invoking `complete` when done.
	virtual void updateImage(sprt::dispatch::Looper *, const Rc<core::DynamicImage> &,
			Vector<FontUpdateRequest> &&, Rc<core::DependencyEvent> &&,
			Function<void(bool)> &&complete) = 0;
};

// Remote endpoint: would forward the font gAPI to the server (atlas compile + glyph raster) and
// receive atlas-ready callbacks. STUB this stage (no transport yet).
class SP_PUBLIC FontGapiProxy final : public FontGapi {
public:
	virtual ~FontGapiProxy();

	virtual void compileImage(const Rc<core::DynamicImage> &, Function<void(bool)> &&) override;
	virtual void updateImage(sprt::dispatch::Looper *, const Rc<core::DynamicImage> &,
			Vector<FontUpdateRequest> &&, Rc<core::DependencyEvent> &&,
			Function<void(bool)> &&complete) override;
};

} // namespace stappler::xenolith::font

#endif /* XENOLITH_FONT_XLFONTGAPI_H_ */
