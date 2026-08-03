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

#include "XLSoftPipeline.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

bool Shader::init(Device &dev, const core::ProgramData &data) {
	_stage = data.stage;
	_programName = data.key.str<Interface>();

	// The SPIR-V blob (data / callback) is deliberately not read: there is no interpreter, and
	// pretending to consume it would only hide a mismatch until nothing draws.
	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::ShaderModule, core::ObjectHandle::zero());
}

bool GraphicPipeline::init(Device &dev, const PipelineData &data) {
	auto &material = data.material;

	// The flat contract has exactly two blend states; anything else means the queue is not the
	// one this backend implements.
	_blendMode = material.getBlendInfo().isEnabled() ? BlendMode::Transparent : BlendMode::Solid;
	_imageType = material.getImageViewType();

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::Pipeline, core::ObjectHandle::zero());
}

} // namespace stappler::xenolith::soft
