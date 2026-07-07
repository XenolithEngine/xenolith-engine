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

#ifndef XENOLITH_BACKEND_WEBGPU_XLWGPU_H_
#define XENOLITH_BACKEND_WEBGPU_XLWGPU_H_

#include "XLCommon.h" // IWYU pragma: keep
#include "XLCoreInstance.h"

// Native (wgpu-native) API extensions are unavailable in a browser build:
// no <webgpu/wgpu.h>, no WGPUNativeFeature_*, no binding arrays, no SPIR-V
// passthrough, no wgpuDevicePoll / wgpuInstanceEnumerateAdapters. Code that
// depends on them must be gated by XL_WGPU_NATIVE_API and have a fallback
// driven by Device::getBackendFeatures().
#if __EMSCRIPTEN__ || SPRT_WASM
#define XL_WGPU_NATIVE_API 0
#else
#define XL_WGPU_NATIVE_API 1
#endif

#include <webgpu/webgpu.h>

#if XL_WGPU_NATIVE_API
#include <webgpu/wgpu.h>
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

using core::InstanceApi;
using core::InstanceFlags;

// capability snapshot for feature-dependent paths; a browser device reports
// all native extensions as unavailable and the standard fallbacks engage
struct BackendFeatures {
	// bindless material texture sets (binding_array in WGSL) - wgpu-native
	// TextureBindingArray; browser fallback: bind group per material
	bool textureBindingArrays = false;
	// partially bound binding arrays (array sized by used slots)
	bool partiallyBoundArrays = false;
	// SPIR-V shader modules (wgpuDeviceCreateShaderModuleSpirV); browser
	// accepts WGSL only
	bool spirvShaders = false;
	// synchronous device polling (wgpuDevicePoll with wait) - used by
	// waitIdle and sync readback; browser is callback-only
	bool syncPolling = false;
	// component swizzle in texture views (standard optional feature);
	// fallback: ColorMode swizzle in the fragment shader (SpanData.colorMode)
	bool textureComponentSwizzle = false;
};

inline StringView toStringView(const WGPUStringView &str) {
	if (!str.data) {
		return StringView();
	}
	if (str.length == WGPU_STRLEN) {
		return StringView(str.data);
	}
	return StringView(str.data, str.length);
}

SP_PUBLIC StringView getBackendTypeName(WGPUBackendType);
SP_PUBLIC StringView getAdapterTypeName(WGPUAdapterType);

} // namespace stappler::xenolith::webgpu

#endif /* XENOLITH_BACKEND_WEBGPU_XLWGPU_H_ */
