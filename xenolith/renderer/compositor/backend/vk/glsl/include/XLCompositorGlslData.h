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


#ifndef XENOLITH_RENDERER_COMPOSITOR_BACKEND_VK_GLSL_INCLUDE_XLCOMPOSITORGLSLDATA_H_
#define XENOLITH_RENDERER_COMPOSITOR_BACKEND_VK_GLSL_INCLUDE_XLCOMPOSITORGLSLDATA_H_

#include "sprt_glsl.h"

#ifndef SP_GLSL
namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor::glsl {

using namespace sprt::glsl;

#endif

// The push constants of every compositor draw.
struct CompositorPlaneData {
	uvec4 dst; // x, y, width, height in output pixels, y down; inside the output
	uvec2 srcOrigin; // the image pixel that lands on dst.xy
	uvec2 extent; // the output
	vec4 color; // Solid: the premultiplied fill
	float alpha; // the plane's own alpha
	uint opaque; // the plane's pixels are taken as alpha 1
	uint index; // the plane's image in the descriptor array
	uint padding;
};

#ifndef SP_GLSL
}
#endif

#endif /* XENOLITH_RENDERER_COMPOSITOR_BACKEND_VK_GLSL_INCLUDE_XLCOMPOSITORGLSLDATA_H_ */
