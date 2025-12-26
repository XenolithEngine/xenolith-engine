/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_RENDERER_BASIC2D_GLSL_XL2DSHADERS_H_
#define XENOLITH_RENDERER_BASIC2D_GLSL_XL2DSHADERS_H_

#include "XL2d.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::shaders {

extern SpanView<uint32_t> MaterialFrag;
extern SpanView<uint32_t> MaterialVert;
extern SpanView<uint32_t> MaterialNoBdaVert;
extern SpanView<uint32_t> PseudoSdfFrag;
extern SpanView<uint32_t> PseudoSdfVert;
extern SpanView<uint32_t> PseudoSdfShadowFrag;
extern SpanView<uint32_t> PseudoSdfShadowVert;
extern SpanView<uint32_t> FrameClipperFrag;

extern SpanView<uint32_t> ParticleUpdateComp;

} // namespace stappler::xenolith::basic2d::shaders

#endif /* XENOLITH_RENDERER_BASIC2D_GLSL_XL2DSHADERS_H_ */
