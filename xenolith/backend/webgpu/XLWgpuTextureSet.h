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

#ifndef XENOLITH_BACKEND_WEBGPU_XLWGPUTEXTURESET_H_
#define XENOLITH_BACKEND_WEBGPU_XLWGPUTEXTURESET_H_

#include "XLWgpuObject.h"
#include "XLCoreTextureSet.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

/* Engine's texture set (material textures array) over wgpu-native binding
 * arrays (TextureBindingArray + PartiallyBoundBindingArray features).
 *
 * Bind group layout mirrors the Vulkan convention:
 *   binding 0: binding_array<sampler, SAMPLERS>
 *   binding 1: binding_array<texture_2d<f32>, IMAGES>
 * and is appended to a pipeline layout as the LAST bind group when
 * PipelineLayoutData::textureSetLayout is set */
class SP_PUBLIC TextureSetLayout final : public core::TextureSetLayout {
public:
	static uint32_t getLayoutImageCount(Device &, const core::TextureSetLayoutData &);

	virtual ~TextureSetLayout() = default;

	bool init(Device &, const core::TextureSetLayoutData &);

	WGPUBindGroupLayout getLayout() const { return _layout; }
	Device *getDevice() const { return _device; }

	SpanView<Rc<core::Sampler>> getCompiledSamplers() const { return _compiledSamplers; }

protected:
	using core::Object::init;

	Device *_device = nullptr;
	WGPUBindGroupLayout _layout = nullptr;
	Vector<Rc<core::Sampler>> _compiledSamplers;
};

class SP_PUBLIC TextureSet final : public core::TextureSet {
public:
	virtual ~TextureSet();

	bool init(Device &, const TextureSetLayout &);

	// rebuilds the bind group (bind groups are immutable in WebGPU)
	virtual void write(const core::MaterialLayout &) override;

	WGPUBindGroup getBindGroup() const { return _bindGroup; }

protected:
	using core::Object::init;

	Device *_device = nullptr;
	const TextureSetLayout *_setLayout = nullptr;
	WGPUBindGroup _bindGroup = nullptr;
};

} // namespace stappler::xenolith::webgpu

#endif /* XENOLITH_BACKEND_WEBGPU_XLWGPUTEXTURESET_H_ */
