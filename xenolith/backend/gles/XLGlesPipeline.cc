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

#include "XLGlesPipeline.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

GLenum getGlBlendFactor(core::BlendFactor factor) {
	switch (factor) {
	case core::BlendFactor::Zero: return GL_ZERO; break;
	case core::BlendFactor::One: return GL_ONE; break;
	case core::BlendFactor::SrcColor: return GL_SRC_COLOR; break;
	case core::BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR; break;
	case core::BlendFactor::DstColor: return GL_DST_COLOR; break;
	case core::BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR; break;
	case core::BlendFactor::SrcAlpha: return GL_SRC_ALPHA; break;
	case core::BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA; break;
	case core::BlendFactor::DstAlpha: return GL_DST_ALPHA; break;
	case core::BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA; break;
	}
	return 0;
}

GLenum getGlBlendOp(core::BlendOp op) {
	switch (op) {
	case core::BlendOp::Add: return GL_FUNC_ADD; break;
	case core::BlendOp::Subtract: return GL_FUNC_SUBTRACT; break;
	case core::BlendOp::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT; break;
	// Min/Max have no GL blend equation: a pipeline asking for them is rejected at init.
	default: return 0; break;
	}
}

static void GlesPipeline_clearShader(core::Device *dev, core::ObjectType, core::ObjectHandle handle,
		void *) {
	auto d = static_cast<Device *>(dev);
	if (!d || !d->isAlive()) { return; }

	const GLuint name = glObjectName(handle);
	d->scheduleRelease([t = &d->getTable(), name]() { t->glDeleteShader(name); });
}

static void GlesPipeline_clearProgram(core::Device *dev, core::ObjectType, core::ObjectHandle handle,
		void *) {
	auto d = static_cast<Device *>(dev);
	if (!d || !d->isAlive()) { return; }

	const GLuint name = glObjectName(handle);
	d->scheduleRelease([t = &d->getTable(), name]() { t->glDeleteProgram(name); });
}

// The compile/link log, as a single line for the diagnostics. An empty string when the entrypoint
// is missing or the driver reports nothing - the failure itself is already logged by the caller.
static String readGlLog(const EglTable &table, bool shader, GLuint name) {
	if ((shader && !table.glGetShaderiv) || (!shader && !table.glGetProgramiv)) {
		return String();
	}

	GLint length = 0;
	if (shader) {
		table.glGetShaderiv(name, GL_INFO_LOG_LENGTH, &length);
	} else {
		table.glGetProgramiv(name, GL_INFO_LOG_LENGTH, &length);
	}
	if (length <= 1) {
		return String();
	}

	Bytes log(length);
	GLsizei written = 0;
	if (shader && table.glGetShaderInfoLog) {
		table.glGetShaderInfoLog(name, GLsizei(log.size()), &written,
				reinterpret_cast<char *>(log.data()));
	} else if (!shader && table.glGetProgramInfoLog) {
		table.glGetProgramInfoLog(name, GLsizei(log.size()), &written,
				reinterpret_cast<char *>(log.data()));
	}

	const char *data = reinterpret_cast<const char *>(log.data());
	size_t size = (written > 0) ? size_t(written) : 0;
	while (size > 0 && (data[size - 1] == '\n' || data[size - 1] == '\r')) {
		--size;
	}
	return String(data, size);
}

bool Shader::init(Device &dev, const core::ProgramData &data) {
	_stage = data.stage;
	_name = data.key.str<Interface>();

	if (!data.data.empty()) {
		return setup(dev, data.data);
	} else if (data.callback != nullptr) {
		bool ret = false;
		data.callback(dev,
				[&, this](SpanView<uint32_t> shaderData) { ret = setup(dev, shaderData); });
		return ret;
	}

	log::source().error("gles::Shader", "No source for program: ", _name);
	return false;
}

bool Shader::setup(Device &dev, SpanView<uint32_t> data) {
	if (data.empty()) {
		log::source().error("gles::Shader", "Empty shader source: ", _name);
		return false;
	}

	// GLSL ES is text, not SPIR-V: a module that starts with the SPIR-V magic belongs to another
	// backend (or to a queue built for one), and compiling it here would fail without explaining
	// why. Say so instead.
	if (data.front() == 0x0723'0203) {
		log::source().error("gles::Shader", "SPIR-V is not accepted by the GLES backend: ", _name);
		return false;
	}

	GLenum type = GL_INVALID_ENUM;
	if (hasFlag(_stage, core::ProgramStage::Vertex)) {
		type = GL_VERTEX_SHADER;
	} else if (hasFlag(_stage, core::ProgramStage::Fragment)) {
		type = GL_FRAGMENT_SHADER;
	} else {
		log::source().error("gles::Shader", "Unknown stage for program: ", _name,
				"(non-SPIR-V programs must declare their ProgramInfo.stage)");
		return false;
	}

	auto &table = dev.getTable();
	if (!table.glCreateShader || !table.glShaderSource || !table.glCompileShader
			|| !table.glGetShaderiv) {
		log::source().error("gles::Shader", "GL shader entrypoints are missing");
		return false;
	}

	const GLuint name = table.glCreateShader(type);
	if (name == 0) {
		log::source().error("gles::Shader", "Fail to create a GL shader, error ",
				EGLint(table.eglGetError()));
		return false;
	}

	// data is UTF-8 text packed into uint32_t words with zero padding at the end - trim the
	// padding back and hand the compiler an explicit length. A null `length` would make
	// glShaderSource take the pointer as a C string, and the packing only leaves a terminator
	// when the text does not end on a word boundary: a source whose length is a multiple of four
	// has none, and the driver then lexes whatever follows the allocation. That is what the
	// "syntax error, unexpected $undefined" at a line past the end of the file was - the source
	// was fine, the read ran off it.
	auto code = reinterpret_cast<const char *>(data.data());
	auto codeSize = size_t(data.size()) * sizeof(uint32_t);
	while (codeSize > 0 && code[codeSize - 1] == '\0') { --codeSize; }

	const GLint codeLength = GLint(codeSize);

	GLuint handle = name;
	table.glShaderSource(handle, 1, &code, &codeLength);
	table.glCompileShader(handle);

	GLint status = GL_FALSE;
	table.glGetShaderiv(handle, GL_COMPILE_STATUS, &status);

	// No retry loop here. There used to be one, for a frontend that "flakily rejected a valid
	// source" - but the source was not valid: it was read past its end (see the length above), and
	// whether the byte after the buffer happened to be a zero decided the run. With the length
	// passed explicitly a compile either succeeds or the source is wrong, and re-issuing it would
	// only delay the diagnostic.
	if (status != GL_TRUE) {
		log::source().error("gles::Shader", "Fail to compile ", _name, ": ", readGlLog(table, true, handle));
		if (handle != 0) { table.glDeleteShader(handle); }
		return false;
	}

	_glShader = handle;

	return core::Shader::init(dev, GlesPipeline_clearShader, core::ObjectType::ShaderModule,
			glObjectHandle(_glShader));
}

bool GraphicPipeline::init(Device &dev, const PipelineData &params) {
	_name = params.key.str<Interface>();

	GLuint vertex = 0;
	GLuint fragment = 0;

	for (auto &it : params.shaders) {
		if (!it.data || !it.data->program) {
			log::source().error("gles::GraphicPipeline", _name, ": program is not compiled: ",
					it.data ? it.data->key : StringView("<null>"));
			return false;
		}

		switch (it.data->stage) {
		case core::ProgramStage::Vertex: vertex = static_cast<Shader *>(it.data->program.get())->getGlName(); break;
		case core::ProgramStage::Fragment: fragment = static_cast<Shader *>(it.data->program.get())->getGlName(); break;
		default:
			log::source().error("gles::GraphicPipeline", _name,
					": unsupported shader stage in graphic pipeline");
			return false;
			break;
		}
	}

	if (vertex == 0) {
		log::source().error("gles::GraphicPipeline", _name, ": no vertex shader");
		return false;
	}

	const auto &blend = params.material.getBlendInfo();
	// The only unmappable entries are the min/max blend ops (GL has no equivalent equation); every
	// factor maps, and BlendFactor::Zero lands on GL_ZERO - 0 - so a zero return above is not a
	// failure marker.
	if (blend.enabled && (core::BlendOp(blend.opColor) == core::BlendOp::Min
			|| core::BlendOp(blend.opColor) == core::BlendOp::Max
			|| core::BlendOp(blend.opAlpha) == core::BlendOp::Min
			|| core::BlendOp(blend.opAlpha) == core::BlendOp::Max)) {
		log::source().error("gles::GraphicPipeline", _name, ": blend state is not mappable to GL");
		return false;
	}

	auto &table = dev.getTable();
	if (!table.glCreateProgram || !table.glAttachShader || !table.glLinkProgram
			|| !table.glGetProgramiv) {
		log::source().error("gles::GraphicPipeline", "GL program entrypoints are missing");
		return false;
	}

	const GLuint program = table.glCreateProgram();
	if (program == 0) {
		log::source().error("gles::GraphicPipeline", "Fail to create a GL program, error ",
				EGLint(table.eglGetError()));
		return false;
	}

	table.glAttachShader(program, vertex);
	if (fragment != 0) {
		table.glAttachShader(program, fragment);
	}

	table.glLinkProgram(program);

	GLint status = GL_FALSE;
	table.glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		log::source().error("gles::GraphicPipeline", "Fail to link ", _name, ": ",
				readGlLog(table, false, program));
		table.glDeleteProgram(program);
		return false;
	}

	// Per-draw uniforms of the flat shaders: GL has no base instance and no per-view swizzle, so
	// both arrive from the executor (see XLGlesQueuePass). A shader without one simply never
	// queries it.
	if (table.glGetUniformLocation) {
		const GLint firstInstance = table.glGetUniformLocation(program, "uFirstInstance");
		if (firstInstance >= 0) {
			_firstInstance = firstInstance;
		}

		const GLint swizzle = table.glGetUniformLocation(program, "uSwizzle");
		if (swizzle >= 0) {
			_swizzle = swizzle;
		}
	}

	_blend = blend;
	_program = program;

	return core::GraphicPipeline::init(dev, GlesPipeline_clearProgram, core::ObjectType::Pipeline,
			glObjectHandle(_program));
}

} // namespace stappler::xenolith::gles
