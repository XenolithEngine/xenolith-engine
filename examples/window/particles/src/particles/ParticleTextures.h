/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#ifndef EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLETEXTURES_H_
#define EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLETEXTURES_H_

#include "XLCommon.h" // IWYU pragma: keep
#include "XLTexture.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Director;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// White disc with a smooth alpha falloff, generated in place so the example ships no image files.
// Null before the director has a resource cache.
Rc<Texture> makeSoftCircleTexture(Director *, uint32_t size = 64);

// A white square with a soft one-texel edge
Rc<Texture> makeSquareTexture(Director *, uint32_t size = 32);

// A soft ellipse elongated along Y, for AlignWithVelocity
Rc<Texture> makeSparkTexture(Director *, uint32_t width = 16, uint32_t height = 64);

// A 4x4 sheet of animation frames, each frame a solid color of getFrameSheetColor, row 0 at the top
Rc<Texture> makeFrameSheetTexture(Director *, uint32_t cellSize = 64);

// Color of frame `index` (0..15) of the frame sheet: distinct, opaque, easy to find in a screenshot
Color3B getFrameSheetColor(uint32_t index);

} // namespace stappler::xenolith::examples

#endif /* EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLETEXTURES_H_ */
