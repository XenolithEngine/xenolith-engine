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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFTPIPELINE_H_
#define XENOLITH_BACKEND_SOFT_XLSOFTPIPELINE_H_

#include "XLSoftDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

// SPIR-V is never interpreted. A shader is a named key: the queue asks for `Loader_FlatVert` /
// `Loader_FlatFrag` and the rasterizer answers with its built-in C++ stages, the same way the
// WebGPU backend answers with WGSL. Anything else is rejected at compileQueue time, loudly,
// rather than silently rendering nothing.
class SP_PUBLIC Shader final : public core::Shader {
public:
	virtual ~Shader() = default;

	bool init(Device &, const core::ProgramData &);

	// name the queue registered the program under; this is the whole "program"
	StringView getProgramName() const { return _programName; }

protected:
	String _programName;
};

// A pipeline is the small set of switches the kernels actually branch on. Everything else the
// PipelineData carries (depth, stencil, cull, polygon mode, sample shading) has no meaning in
// the flat contract and is dropped here on purpose.
class SP_PUBLIC GraphicPipeline final : public core::GraphicPipeline {
public:
	virtual ~GraphicPipeline() = default;

	bool init(Device &, const PipelineData &);

	BlendMode getBlendMode() const { return _blendMode; }
	core::ImageViewType getImageType() const { return _imageType; }

protected:
	BlendMode _blendMode = BlendMode::Solid;
	core::ImageViewType _imageType = core::ImageViewType::ImageView2D;
};

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFTPIPELINE_H_ */
