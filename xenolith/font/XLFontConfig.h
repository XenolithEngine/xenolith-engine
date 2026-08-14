/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_FONT_XLFONTCONFIG_H_
#define XENOLITH_FONT_XLFONTCONFIG_H_

#include "XLCommon.h"
#include "SPFontStyle.h"
#include "SPFontFace.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::config {

// max chars count, used by locale::hasLocaleTagsFast
static constexpr size_t MaxFastLocaleChars = size_t(127);

// How full the glyph cache has to be before FontController::update() starts dropping the font sets
// nobody holds any more. Below it a spare set is kept: it costs nothing while there is room, and
// re-creating one costs a full atlas rebuild - a new image, every non-persistent glyph rasterized
// again, and a material recompile in every window.
static constexpr float FontCacheEvictionThreshold = 0.75f;

// What "full" means for the atlas image. It cannot be a fill RATIO of the current image: the packer
// re-picks the extent from scratch on every rebuild as the smallest 128*2^k box that fits the
// glyphs (font::emplaceChars), so the image is always a tight fit and its fill ratio measures how
// well the packer did, not how much is cached. What does grow with the cache is the image itself,
// so that is what is measured - the atlas is R8_UNORM, one byte per texel, so this is both its area
// and its size on the GPU.
static constexpr uint64_t FontCacheAtlasBudget = uint64_t(1'024) * uint64_t(1'024);

// The same question for a controller that has no atlas image to measure: the software rasterizer
// keeps every glyph as its own texture, and a remote client leaves the atlas to the server. Bounding
// the number of live sets is what keeps those from growing without end - and it is also the guard
// against exhausting the 14-bit face-id space that CharIds are baked from (FontLibrary::getNextId
// aborts when it runs out, and an id is only released when its face is reaped).
static constexpr size_t FontCacheMaxLayouts = size_t(256);

} // namespace stappler::xenolith::config

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

using namespace stappler::font;

}

#endif /* XENOLITH_FONT_XLFONTCONFIG_H_ */
