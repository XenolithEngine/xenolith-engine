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
#include "SPRaster.h" // IWYU pragma: keep

// The software backend executes the flat 2d render queue on the CPU: there is no driver, no
// shader compiler and no device memory - "device memory" is malloc'd, an image is a linear
// bitmap and a pipeline is a key that selects a C++ kernel.
//
// Only what the flat queue needs is implemented; see soft-raster-backend-plan.md for the
// contract and for what is deliberately out of scope (depth/stencil, MSAA, compute, sRGB).

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

// The rasterizer is a stappler module now (it depends on nothing but stappler_core and is useful
// outside a renderer). The alias keeps the backend and the 2d renderer spelling it `soft::raster`,
// which is where it reads best from their side.
namespace raster = ::stappler::raster;

class Instance;
class Device;
class Loop;

// What the rasterizer is allowed to do on this build. Filled once, at Loop init.
struct SP_PUBLIC BackendFeatures {
	// Number of worker threads the rasterizer may fan tiles out to. M0 rasterizes inline on the
	// loop thread, so this is informational until tiling lands.
	uint32_t threadCount = 1;
};

// The blend state is the rasterizer's, not the backend's; the backend only picks one.
using raster::BlendMode;

// core::ImageFormat -> what the rasterizer can address. This is the boundary translation, and it
// belongs here: it happens once per image and once per texture, never per pixel.
inline raster::PixelFormat getRasterFormat(core::ImageFormat format) {
	switch (format) {
	case core::ImageFormat::R8_UNORM: return raster::PixelFormat::R8;
	case core::ImageFormat::R8G8B8A8_UNORM: return raster::PixelFormat::RGBA8888;
	case core::ImageFormat::B8G8R8A8_UNORM: return raster::PixelFormat::BGRA8888;
	default: return raster::PixelFormat::Undefined;
	}
}

// Bytes per pixel for a format the backend accepts. Returns 0 for anything it can not address
// linearly, which is how callers detect an unsupported image.
inline uint32_t getPixelSize(core::ImageFormat format) {
	return raster::getPixelSize(getRasterFormat(format));
}

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFT_H_ */
