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

#ifndef XENOLITH_FONT_BACKEND_GLES_XLGLESFONTQUEUE_H_
#define XENOLITH_FONT_BACKEND_GLES_XLGLESFONTQUEUE_H_

#include "XLFontComponent.h"

#if MODULE_XENOLITH_BACKEND_GLES

#include "XLGlesQueuePass.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

/* Font glyph atlas queue for the OpenGL ES backend.
 *
 * Glyphs are rasterized on CPU (FreeType), packed into an R8 atlas with
 * font::emplaceChars, composed into a tightly-packed byte buffer, and uploaded
 * as a single glTexSubImage2D via gles::Image's initialData path. The 2d flat
 * pass samples the resulting texture with per-glyph UVs resolved from the
 * DataAtlas on CPU (hasGpuSideAtlases = false). */
class SP_PUBLIC FontQueue : public core::Queue {
public:
	virtual ~FontQueue();

	bool init(StringView name);

	const core::AttachmentData *getAttachment() const { return _attachment; }

protected:
	using core::Queue::init;

	const core::AttachmentData *_attachment = nullptr;
};

} // namespace stappler::xenolith::gles

#endif /* MODULE_XENOLITH_BACKEND_GLES */

#endif /* XENOLITH_FONT_BACKEND_GLES_XLGLESFONTQUEUE_H_ */
