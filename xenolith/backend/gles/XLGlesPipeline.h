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

#ifndef XENOLITH_BACKEND_GLES_XLGLESPIPELINE_H_
#define XENOLITH_BACKEND_GLES_XLGLESPIPELINE_H_

#include "XLGlesDevice.h"
#include "XLCoreQueueData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// core blend vocabulary -> the GL enums a pipeline applies at bind time. A zero result is how
// callers reject a value GLES has no equation for (Min/Max are not blend operations in GL).
SP_PUBLIC GLenum getGlBlendFactor(core::BlendFactor);
SP_PUBLIC GLenum getGlBlendOp(core::BlendOp);

// One compiled stage of a program: ProgramData carries the GLSL ES source as UTF-8 packed into
// uint32_t words (inspect() leaves non-SPIR-V programs alone, so the stage comes from the
// explicit ProgramInfo the queue builder was given). The object exists per the core contract;
// the pass executor only needs the linked program.
class SP_PUBLIC Shader final : public core::Shader {
public:
	virtual ~Shader() = default;

	bool init(Device &dev, const core::ProgramData &);

	GLuint getGlName() const { return _glShader; }

protected:
	bool setup(Device &dev, SpanView<uint32_t> data);

	GLuint _glShader = 0;
};

// A linked program plus the plain state the pass executor applies for it. The blend description
// is stored as-is (it is uint32-sized and comparable), so a change between two pipelines shows up
// in one equality check; depth is deliberately not applied - the flat contract has no depth
// attachment, exactly as in the software backend, and enabling DEPTH_TEST without a depth buffer
// would make draws undefined.
class SP_PUBLIC GraphicPipeline final : public core::GraphicPipeline {
public:
	virtual ~GraphicPipeline() = default;

	bool init(Device &dev, const PipelineData &);

	GLuint getGlName() const { return _program; }

	const core::BlendInfo &getBlendInfo() const { return _blend; }

	// Locations of the per-draw uniforms, or -1 when the shader does not declare them. The pass
	// executor sets whatever exists on every draw: uFirstInstance is the GL stand-in for a base
	// instance (gl_InstanceID carries only the span-local number), and uSwizzle applies the view's
	// component mapping in the fragment stage - GLES has no per-view swizzle, so it travels with
	// the draw instead.
	GLint getFirstInstanceLocation() const { return _firstInstance; }
	GLint getSwizzleLocation() const { return _swizzle; }

protected:
	GLuint _program = 0;
	core::BlendInfo _blend;
	GLint _firstInstance = -1;
	GLint _swizzle = -1;
};

} // namespace stappler::xenolith::gles

#endif /* XENOLITH_BACKEND_GLES_XLGLESPIPELINE_H_ */
