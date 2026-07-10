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

#ifndef XENOLITH_BACKEND_MTL_XLMTL_H_
#define XENOLITH_BACKEND_MTL_XLMTL_H_

#include "XLCommon.h" // IWYU pragma: keep
#include "XLCoreInstance.h"

// The backend is built as a single Objective-C++ SCU (XLMtl.scu.mm) with ARC.
// Class headers stay plain C++ so that dependent C++ TUs can include them:
// every Metal object is held as an opaque `void *` bridged with
// __bridge_retained on creation and __bridge_transfer on destruction (the
// core::Object ClearCallback). Typed accessors are exposed only to ObjC++
// includers under __OBJC__.
#if __OBJC__
// SDK headers self-reference deprecated and newer-than-deployment-target
// APIs; suppressed the same way the runtime's own macOS sources do
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#pragma clang diagnostic pop
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

using core::InstanceApi;
using core::InstanceFlags;

// Shaders are native MSL: core::ProgramData::data carries UTF-8 Metal Shading
// Language text (padded to uint32_t words, as WGSL in the WebGPU backend).
// SPIR-V modules are rejected - queues intended for this backend must provide
// MSL sources. MSL reserves `main`, so the entry point is `main0`
// (the MoltenVK/SPIRV-Cross convention).
inline constexpr const char *ShaderEntryPointName = "main0";

// Binding conventions for MSL programs (Metal has no descriptor sets, the
// engine's descriptors map to per-stage argument table indexes):
//  - buffer descriptors take [[buffer(N)]] slots in declaration order from 0
//  - sampled images take [[texture(N)]], samplers take [[sampler(N)]]
//  - the texture set argument buffer is bound at [[buffer(30)]] (the last
//    slot of the 31-entry buffer argument table)
inline constexpr uint32_t TextureSetBufferIndex = 30;

// retain an ObjC object into an opaque handle (release with releaseHandle)
#if __OBJC__
inline void *retainHandle(id object) { return (__bridge_retained void *)object; }

inline void releaseHandle(void *handle) {
	if (handle) {
		(void)(__bridge_transfer id)handle;
	}
}

template <typename Interface>
inline Interface bridgeHandle(void *handle) {
	return (__bridge Interface)handle;
}
#endif

} // namespace stappler::xenolith::mtl

#endif /* XENOLITH_BACKEND_MTL_XLMTL_H_ */
