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

#ifndef XENOLITH_BACKEND_MTL_XLMTLPIPELINE_H_
#define XENOLITH_BACKEND_MTL_XLMTLPIPELINE_H_

#include "XLMtlDevice.h"
#include "XLCoreQueueData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

#if __OBJC__
SP_PUBLIC MTLPixelFormat getMTLPixelFormat(core::ImageFormat);
SP_PUBLIC MTLBlendFactor getMTLBlendFactor(core::BlendFactor);
SP_PUBLIC MTLBlendOperation getMTLBlendOperation(core::BlendOp);
SP_PUBLIC MTLCompareFunction getMTLCompareFunction(core::CompareOp);
#endif

// Shader with native MSL source; ProgramData::data carries UTF-8 Metal Shading
// Language text (padded to uint32_t words). SPIR-V modules are rejected: the
// backend performs no translation, queues must ship MSL variants of their
// programs. The entry point is `main0` (`main` is reserved in MSL).
class SP_PUBLIC Shader final : public core::Shader {
public:
	virtual ~Shader() = default;

	bool init(Device &dev, const core::ProgramData &);

#if __OBJC__
	id<MTLLibrary> getLibrary() const { return bridgeHandle<id<MTLLibrary>>(_library); }
	id<MTLFunction> getFunction() const { return bridgeHandle<id<MTLFunction>>(_function); }
#endif

protected:
	bool setup(Device &dev, const core::ProgramData &, SpanView<uint32_t>);

	void *_library = nullptr; // __bridge_retained id<MTLLibrary>
	void *_function = nullptr; // __bridge_retained id<MTLFunction>
};

class SP_PUBLIC GraphicPipeline final : public core::GraphicPipeline {
public:
	virtual ~GraphicPipeline() = default;

	bool init(Device &dev, const PipelineData &);

#if __OBJC__
	id<MTLRenderPipelineState> getPipeline() const {
		return bridgeHandle<id<MTLRenderPipelineState>>(_pipeline);
	}
	id<MTLDepthStencilState> getDepthStencil() const {
		return bridgeHandle<id<MTLDepthStencilState>>(_depthStencil);
	}
#endif

protected:
	void *_pipeline = nullptr; // __bridge_retained id<MTLRenderPipelineState>
	// depth/stencil is a separate immutable state object in Metal, captured
	// from the pipeline's MaterialInfo alongside the render pipeline itself
	void *_depthStencil = nullptr; // __bridge_retained id<MTLDepthStencilState>
};

class SP_PUBLIC ComputePipeline final : public core::ComputePipeline {
public:
	virtual ~ComputePipeline() = default;

	bool init(Device &dev, const PipelineData &);

#if __OBJC__
	id<MTLComputePipelineState> getPipeline() const {
		return bridgeHandle<id<MTLComputePipelineState>>(_pipeline);
	}
#endif

protected:
	void *_pipeline = nullptr; // __bridge_retained id<MTLComputePipelineState>
};

} // namespace stappler::xenolith::mtl

#endif /* XENOLITH_BACKEND_MTL_XLMTLPIPELINE_H_ */
