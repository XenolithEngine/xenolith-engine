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

#ifndef XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORBLEND_H_
#define XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORBLEND_H_

#include "XLCommon.h"
#include "SPRaster.h"

// The CPU half of the compositor: planes blended into a target, nothing else. It knows no window,
// no queue and no thread, so the same function serves a soft pass and a framebuffer compositor.

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor {

enum class PlaneBlend : uint8_t {
	// the plane covers what is below it; its alpha is taken as 1
	Opaque,
	// the plane holds premultiplied colour: dst = src + dst * (1 - srcAlpha)
	Premultiplied,
};

// One plane's pixels and where they land. The source is copied 1:1: `src` is the part of the image
// that is shown and `origin` is where its top-left pixel goes in the target.
struct SP_PUBLIC BlendSource {
	const uint8_t *pixels = nullptr;
	uint32_t stride = 0;
	raster::PixelFormat format = raster::PixelFormat::Undefined;
	URect src;
	IVec2 origin;
	uint8_t alpha = 255; // the plane's own alpha, applied on top of the pixels'
	PlaneBlend blend = PlaneBlend::Opaque;
};

// Compose `clip` of the target: the background, then the sources bottom to top. The target and the
// sources are RGBA8888 or BGRA8888; a source in another format is skipped. The background is
// straight colour and is premultiplied here.
SP_PUBLIC void blendPlanes(const raster::Target &, SpanView<BlendSource> bottomToTop,
		const Color4F &background, const URect &clip);

} // namespace stappler::xenolith::compositor

#endif /* XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORBLEND_H_ */
