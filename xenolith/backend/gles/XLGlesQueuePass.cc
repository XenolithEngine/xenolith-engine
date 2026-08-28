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

#include "XLGlesQueuePass.h"
#include "XLGlesObject.h"
#include "XLGlesPipeline.h"

// Desktop-GL token, absent from the GLES3 headers (the diagnostic query it names is too).

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

bool RenderPass::init(Device &dev, const core::QueuePassData &) {
	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::RenderPass, core::ObjectHandle::zero());
}

bool CommandBuffer::init(Device &dev) {
	_device = &dev;
	return true;
}

void QueuePassHandle::recordSubpass(core::FrameQueue &q, const core::SubpassData &subpass,
		CommandBuffer &buf) {
	if (subpass.commandsCallback) {
		subpass.commandsCallback(q, subpass, buf);
	}
}

URect QueuePassHandle::rotateScissor(const core::FrameConstraints &constraints,
		const URect &scissor) {
	// Y flip first: scene space grows upwards, the target downwards.
	int32_t x = int32_t(scissor.x);
	int32_t y = int32_t(constraints.extent.height - scissor.y - scissor.height);
	uint32_t width = scissor.width;
	uint32_t height = scissor.height;

	switch (core::getPureTransform(constraints.transform)) {
	case core::SurfaceTransformFlags::Rotate90:
		y = int32_t(scissor.x);
		x = int32_t(scissor.y);
		sprt::swap(width, height);
		break;
	case core::SurfaceTransformFlags::Rotate180: y = int32_t(scissor.y); break;
	case core::SurfaceTransformFlags::Rotate270:
		y = int32_t(constraints.extent.height - scissor.x - scissor.width);
		x = int32_t(constraints.extent.width - scissor.y - scissor.height);
		sprt::swap(width, height);
		break;
	default: break;
	}

	if (x < 0) {
		width = (uint32_t(-x) < width) ? width - uint32_t(-x) : 0;
		x = 0;
	}

	if (y < 0) {
		height = (uint32_t(-y) < height) ? height - uint32_t(-y) : 0;
		y = 0;
	}

	return URect{uint32_t(x), uint32_t(y), width, height};
}

bool QueuePassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	_device = static_cast<Device *>(q.getFrame()->getDevice());

	return core::QueuePassHandle::prepare(q, sp::move(cb));
}

// Apply the pass's load ops to its framebuffer. The attachment list and the framebuffer are built
// from the same ordered vector (the frame queue collects the views while walking _data->attachments),
// so position i here is GL_COLOR_ATTACHMENT0 + i there - no lookup needed.
bool QueuePassHandle::runPass(core::FrameQueue &q) {
	auto fbo = static_cast<const Framebuffer *>(getFramebuffer());
	if (!fbo || !fbo->isComplete()) {
		log::source().error("gles::QueuePassHandle", "No usable framebuffer for pass: ",
				getName());
		return false;
	}

	auto &table = _device->getTable();
	if (!table.glBindFramebuffer || !table.glViewport) {
		log::source().error("gles::QueuePassHandle", "GL framebuffer entrypoints are missing");
		return false;
	}

	table.glBindFramebuffer(GL_FRAMEBUFFER, fbo->getGlName());
	auto extent = fbo->getExtent();
	table.glViewport(0, 0, GLsizei(extent.width), GLsizei(extent.height));

	bool ok = true;
	// Load ops must see the whole attachment: GL applies the scissor test to glClear*, and a scissor
	// left enabled by the previous frame's draws would clip the clear to that rectangle, keeping every
	// stale pixel outside it (the "clip" parity case). executeDrawList re-enables the test per its
	// first scissored draw, so disabling here is enough.
	table.glDisable(GL_SCISSOR_TEST);
	for (size_t i = 0; i < _data->attachments.size() && ok; ++i) {
		const auto *att = _data->attachments[i];
		if (att->loadOp == core::AttachmentLoadOp::Clear) {
			auto imgAtt = static_cast<const core::ImageAttachment *>(
					att->attachment->attachment.get());
			auto color = imgAtt->getClearColor();
			float value[4] = { color.r, color.g, color.b, color.a };
			if (!table.glClearBufferfv) {
				log::source().error("gles::QueuePassHandle", "glClearBufferfv is missing");
				ok = false;
				break;
			}
			table.glClearBufferfv(GL_COLOR, GLint(i), value);
		} else if (att->loadOp == core::AttachmentLoadOp::DontCare && table.glInvalidateFramebuffer) {
			const GLenum attachment = GL_COLOR_ATTACHMENT0 + GLsizei(i);
			table.glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &attachment);
		}
	}

	if (ok) {
		for (auto &subpass : _data->subpasses) {
			auto buf = Rc<CommandBuffer>::create(*_device);
			if (!buf) {
				log::source().error("gles::QueuePassHandle", "Fail to create a command buffer: ",
						subpass->key);
				ok = false;
				break;
			}

			recordSubpass(q, *subpass, *buf);


			if (buf->getDraws().empty()) {
				continue; // nothing was recorded for this subpass - the load ops stand as is
			}

			if (!executeDrawList(*buf, extent.height)) {
				ok = false;
				break;
			}
		}
	}


	table.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return ok;
}

// One pass over a recorded buffer. The vertex format is whatever the recorder described (the flat
// contract's five attributes on a 48-byte stride), so nothing here knows what a frame looks like.
bool QueuePassHandle::executeDrawList(const CommandBuffer &buf, uint32_t targetHeight) {
	auto &table = _device->getTable();
	if (!table.hasGlDraw()) {
		log::source().error("gles::QueuePassHandle", "GL draw entrypoints are missing");
		return false;
	}

	if (buf.vertexBuffer == 0 || buf.indexBuffer == 0) {
		log::source().error("gles::QueuePassHandle", "A recorded buffer has no vertex/index data");
		return false;
	}


	GLuint vao = 0;
	table.glGenVertexArrays(1, &vao);
	if (vao == 0) {
		log::source().error("gles::QueuePassHandle", "Fail to create a VAO, error ",
				EGLint(table.eglGetError()));
		return false;
	}


	table.glBindVertexArray(vao);
	table.glBindBuffer(GL_ARRAY_BUFFER, buf.vertexBuffer);
	for (const auto &attr : buf.vertexAttributes) {
		// Integer-typed attributes must be set through glVertexAttribIPointer; feeding them via the
		// float entrypoint is undefined behavior in ES 3.0 and this driver delivers garbage (or zeros)
		// for them, which corrupts the material/object words the flat vertex shader reads. The IPointer
		// call takes no normalized argument - integer arrays are never normalized.
		const bool isIntegerType = attr.type == GL_BYTE || attr.type == GL_UNSIGNED_BYTE ||
				attr.type == GL_SHORT || attr.type == GL_UNSIGNED_SHORT ||
				attr.type == GL_INT || attr.type == GL_UNSIGNED_INT;
		if (isIntegerType && table.glVertexAttribIPointer) {
			table.glVertexAttribIPointer(attr.location, attr.size, attr.type, GLsizei(buf.vertexStride),
					reinterpret_cast<GLvoid *>(uintptr_t(attr.offset)));
		} else {
			table.glVertexAttribPointer(attr.location, attr.size, attr.type, attr.normalized ? 1 : 0,
					GLsizei(buf.vertexStride), reinterpret_cast<GLvoid *>(uintptr_t(attr.offset)));
		}
		table.glEnableVertexAttribArray(attr.location);
	}
	table.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf.indexBuffer);


	if (buf.transformBuffer != 0) {
		table.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf.transformBuffer);
	}


	bool success = true;
	GLuint lastProgram = ~GLuint(0);
	const core::BlendInfo *lastBlend = nullptr; // state follows the program: null until one is bound
	GLuint lastTexture = 0;
	GLuint lastSampler = 0;
	GLint firstInstanceValue = -1;
	int swizzle[4] = { -1, -1, -1, -1 };
	URect lastScissor{~uint32_t(0), ~uint32_t(0), 0, 0}; // a value no real rect can hold

	for (const auto &draw : buf.getDraws()) {
		auto pipeline = draw.pipeline.get_cast<GraphicPipeline>();
		if (!pipeline || draw.indexCount == 0) {
			continue;
		}

		const GLuint program = pipeline->getGlName();
		if (program != lastProgram) {
			table.glUseProgram(program);
			lastProgram = program;
			lastBlend = nullptr;
			firstInstanceValue = -1;
			swizzle[0] = swizzle[1] = swizzle[2] = swizzle[3] = -1;
		}

		if (!lastBlend || *lastBlend != pipeline->getBlendInfo()) {
			const auto &blend = pipeline->getBlendInfo();
			if (blend.isEnabled()) {
				table.glEnable(GL_BLEND);
				table.glBlendFuncSeparate(getGlBlendFactor(core::BlendFactor(blend.srcColor)),
						getGlBlendFactor(core::BlendFactor(blend.dstColor)),
						getGlBlendFactor(core::BlendFactor(blend.srcAlpha)),
						getGlBlendFactor(core::BlendFactor(blend.dstAlpha)));
				table.glBlendEquationSeparate(getGlBlendOp(core::BlendOp(blend.opColor)),
						getGlBlendOp(core::BlendOp(blend.opAlpha)));
			} else {
				table.glDisable(GL_BLEND);
			}

			const auto mask = core::ColorComponentFlags(blend.writeMask);
			table.glColorMask(hasFlag(mask, core::ColorComponentFlags::R),
					hasFlag(mask, core::ColorComponentFlags::G),
					hasFlag(mask, core::ColorComponentFlags::B),
					hasFlag(mask, core::ColorComponentFlags::A));

			lastBlend = &blend;
		}

		if (draw.scissor.x != lastScissor.x || draw.scissor.y != lastScissor.y
				|| draw.scissor.width != lastScissor.width
				|| draw.scissor.height != lastScissor.height) {
			table.glEnable(GL_SCISSOR_TEST);
			// rotateScissor reports the rect in top-left origin (the software backend's bitmap
			// convention). glScissor measures its lower-left corner from the bottom of the
			// framebuffer, so flip Y against the target height - the same reason the vertex shader
			// negates gl_Position.y. Without this a clip would cut out the vertically mirrored
			// region instead of the one it was asked to keep.
			const GLint scissorY = GLint(targetHeight) - GLint(draw.scissor.y) - GLint(draw.scissor.height);
			table.glScissor(GLint(draw.scissor.x), scissorY,
					GLsizei(draw.scissor.width), GLsizei(draw.scissor.height));
			lastScissor = draw.scissor;
		}

		if (draw.texture != lastTexture || draw.sampler != lastSampler) {
			table.glActiveTexture(GL_TEXTURE0);
			table.glBindTexture(GL_TEXTURE_2D, draw.texture);
			table.glBindSampler(0, draw.sampler); // 0 leaves the unit's sampler unbound
			lastTexture = draw.texture;
			lastSampler = draw.sampler;
		}

		if (pipeline->getFirstInstanceLocation() >= 0 && GLint(draw.firstInstance) != firstInstanceValue) {
			table.glUniform1i(pipeline->getFirstInstanceLocation(), GLint(draw.firstInstance));
			firstInstanceValue = GLint(draw.firstInstance);
		}

		if (pipeline->getSwizzleLocation() >= 0) {
			bool changed = false;
			for (int i = 0; i < 4; ++i) {
				if (draw.swizzle[i] != swizzle[i]) { changed = true; break; }
			}
			if (changed) {
				table.glUniform4i(pipeline->getSwizzleLocation(), draw.swizzle[0], draw.swizzle[1],
						draw.swizzle[2], draw.swizzle[3]);
				sprt::memcpy(swizzle, draw.swizzle, sizeof(swizzle));
			}
		}


		const void *indices = reinterpret_cast<const void *>(uintptr_t(draw.firstIndex) * 4);

		if (draw.instanceCount > 1) {
			table.glDrawElementsInstanced(GL_TRIANGLES, GLsizei(draw.indexCount), GL_UNSIGNED_INT,
					indices, GLsizei(draw.instanceCount));
		} else {
			table.glDrawElements(GL_TRIANGLES, GLsizei(draw.indexCount), GL_UNSIGNED_INT, indices);
		}

	}

	table.glBindVertexArray(0);
	return success;
}

void QueuePassHandle::submit(core::FrameQueue &q, Rc<core::FrameSync> &&sync,
		Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {
	// Objects dropped on any thread since the last frame wait here for their GL delete; doing it
	// before this pass's work keeps one thread owning every API call.
	if (_device) { _device->drainPendingReleases(); }

	auto success = runPass(q);

	// The frame graph learns about completion through the fence's release callbacks, so a failed
	// pass still has to produce one - otherwise the frame would stall in Submission state with no
	// diagnostics (the base handle fails for exactly this reason).
	_fence = _loop->acquireFence(core::FenceType::Default);
	if (!_fence) {
		onSubmited(false);
		return;
	}

	// Order the pass's completion against its resources with a GL fence created after all of the
	// work above. A host-only fence (the failure path) reads as signalled at once, which is what
	// the callbacks expect when nothing was actually submitted.
	if (success && _device) {
		auto &table = _device->getTable();
		GLsync glSync = nullptr;
		if (!table.glFenceSync || (glSync = table.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
				== nullptr) {
			log::source().error("gles::QueuePassHandle", "Fail to create a GL fence sync");
			success = false;
		} else if (auto fence = _fence.get_cast<Fence>()) {
			fence->setGlSync(glSync); // attaches and arms: unsignalled until glClientWaitSync sees it
		}
	}

	_fence->setTag(getName());
	_fence->addRelease([this, guard = Rc<core::FrameQueue>(&q),
							   onComplete = sp::move(onComplete)](bool fenceSuccess) {
		for (auto &it : _data->completeCallbacks) { it(*guard, *_data, fenceSuccess); }
		onComplete(fenceSuccess);
	}, this, "gles::QueuePassHandle::submit");

	// Nothing armed this fence on a device queue - there is no queue - so arm it by hand. Without
	// this core::Fence::check short-circuits on a non-Armed state and the release callbacks never
	// run (the soft backend needs the same call for the same reason).
	_fence->setArmed();

	for (auto &it : _data->submittedCallbacks) { it(q, *_data, success); }

	onSubmited(success);

	auto fence = move(_fence);
	_fence = nullptr;
	fence->schedule(*_loop);
}

} // namespace stappler::xenolith::gles
