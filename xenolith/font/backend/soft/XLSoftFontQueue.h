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

#ifndef XENOLITH_FONT_BACKEND_SOFT_XLSOFTFONTQUEUE_H_
#define XENOLITH_FONT_BACKEND_SOFT_XLSOFTFONTQUEUE_H_

#include "XLFontComponent.h"

#if MODULE_XENOLITH_BACKEND_SOFT

#include "XLSoftQueuePass.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

/* Glyph queue for the software backend.
 *
 * It builds no atlas image. Where the other backends pack glyphs into a texture and upload it, this
 * one rasterizes each new glyph once, in place, into a GlyphStore that survives between frames, and
 * publishes only the quad geometry - the DataAtlas keeps its role as the table of per-glyph offsets
 * while its texture coordinates become the glyph-local unit square. See XLSoftGlyphStore.h for why
 * an atlas is a cost rather than a saving here.
 *
 * The image attached to the dynamic image is a 1x1 placeholder: materials need one, nothing samples
 * it. The store travels alongside as the instance's userdata, which is the same seam the Vulkan
 * queue uses for its persistent glyph buffers. */
class SP_PUBLIC FontQueue : public core::Queue {
public:
	virtual ~FontQueue();

	bool init(StringView name);

	const core::AttachmentData *getAttachment() const { return _attachment; }

protected:
	using core::Queue::init;

	const core::AttachmentData *_attachment = nullptr;
};

} // namespace stappler::xenolith::soft

#endif

#endif /* XENOLITH_FONT_BACKEND_SOFT_XLSOFTFONTQUEUE_H_ */
