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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFT_H_
#define XENOLITH_BACKEND_SOFT_XLSOFT_H_

#include "XLCore.h" // IWYU pragma: keep
#include "XLCoreInfo.h" // IWYU pragma: keep

// The software backend executes the flat 2d render queue on the CPU: there is no driver, no
// shader compiler and no device memory - "device memory" is malloc'd, an image is a linear
// bitmap and a pipeline is a key that selects a C++ kernel.
//
// Only what the flat queue needs is implemented; see soft-raster-backend-plan.md for the
// contract and for what is deliberately out of scope (depth/stencil, MSAA, compute, sRGB).

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

class Instance;
class Device;
class Loop;

// What the rasterizer is allowed to do on this build. Filled once, at Loop init.
struct SP_PUBLIC BackendFeatures {
	// Number of worker threads the rasterizer may fan tiles out to. M0 rasterizes inline on the
	// loop thread, so this is informational until tiling lands.
	uint32_t threadCount = 1;
};

// Blend modes of the flat contract. There are exactly two, and they are not configurable:
// materials pick one through PipelineMaterialInfo.
enum class BlendMode {
	// blending disabled, plain write
	Solid,
	// color = SrcAlpha/OneMinusSrcAlpha (Add), alpha = Zero/One (Add):
	// destination alpha is preserved and nothing is premultiplied
	Transparent,
};

// Bytes per pixel for a format the backend accepts. Returns 0 for anything it can not address
// linearly, which is how callers detect an unsupported target.
inline uint32_t getPixelSize(core::ImageFormat format) {
	switch (format) {
	case core::ImageFormat::R8_UNORM: return 1;
	case core::ImageFormat::R8G8B8A8_UNORM:
	case core::ImageFormat::B8G8R8A8_UNORM: return 4;
	default: return 0;
	}
}

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFT_H_ */
