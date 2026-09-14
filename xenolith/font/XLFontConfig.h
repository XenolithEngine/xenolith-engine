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

// How full the glyph cache must be before FontController::update() drops font sets nobody holds.
// Below it spare sets are kept: re-creating one costs a full atlas rebuild and material recompiles.
static constexpr float FontCacheEvictionThreshold = 0.75f;

// Atlas image budget, in bytes (R8_UNORM, one byte per texel). Not a fill ratio: the packer picks
// the smallest fitting 128*2^k extent on every rebuild (font::emplaceChars), so the image is always
// tight and its size is what grows with the cache.
static constexpr uint64_t FontCacheAtlasBudget = uint64_t(1'024) * uint64_t(1'024);

// Live set limit for controllers with no atlas image (software rasterizer, remote client). Also
// guards the 14-bit face-id space CharIds are built from: FontLibrary::getNextId aborts when it
// runs out, and ids are released only when a face is reaped.
static constexpr size_t FontCacheMaxLayouts = size_t(256);

} // namespace stappler::xenolith::config

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

using namespace stappler::font;

}

#endif /* XENOLITH_FONT_XLFONTCONFIG_H_ */
