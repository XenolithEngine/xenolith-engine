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

#ifndef XENOLITH_BACKEND_WEBGPU_XLWGPUPIPELINE_H_
#define XENOLITH_BACKEND_WEBGPU_XLWGPUPIPELINE_H_

#include "XLWgpuDevice.h"
#include "XLCoreQueueData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

SP_PUBLIC WGPUTextureFormat getWGPUFormat(core::ImageFormat);
SP_PUBLIC WGPUBlendFactor getWGPUBlendFactor(core::BlendFactor);
SP_PUBLIC WGPUBlendOperation getWGPUBlendOperation(core::BlendOp);
SP_PUBLIC WGPUCompareFunction getWGPUCompareFunction(core::CompareOp);

// Shader with native WGSL source; ProgramData::data carries UTF-8 WGSL text
// (padded to uint32_t words), entry point is always `main`
class SP_PUBLIC Shader final : public core::Shader {
public:
	virtual ~Shader() = default;

	bool init(Device &dev, const core::ProgramData &);

	WGPUShaderModule getModule() const { return _shaderModule; }

protected:
	bool setup(Device &dev, const core::ProgramData &, SpanView<uint32_t>);

	WGPUShaderModule _shaderModule = nullptr;
};

class SP_PUBLIC GraphicPipeline final : public core::GraphicPipeline {
public:
	virtual ~GraphicPipeline() = default;

	bool init(Device &dev, const PipelineData &);

	WGPURenderPipeline getPipeline() const { return _pipeline; }

protected:
	WGPURenderPipeline _pipeline = nullptr;
};

class SP_PUBLIC ComputePipeline final : public core::ComputePipeline {
public:
	virtual ~ComputePipeline() = default;

	bool init(Device &dev, const PipelineData &);

	WGPUComputePipeline getPipeline() const { return _pipeline; }

protected:
	WGPUComputePipeline _pipeline = nullptr;
};

} // namespace stappler::xenolith::webgpu

#endif /* XENOLITH_BACKEND_WEBGPU_XLWGPUPIPELINE_H_ */
