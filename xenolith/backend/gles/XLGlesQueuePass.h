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

#ifndef XENOLITH_BACKEND_GLES_XLGLESQUEUEPASS_H_
#define XENOLITH_BACKEND_GLES_XLGLESQUEUEPASS_H_

#include "XLGlesDevice.h"
#include "XLCoreQueuePass.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// The render pass has no state to hold: it only identifies the pass inside the frame graph, the
// same way the soft one does. Its index is what the framebuffer cache keys on.
class SP_PUBLIC RenderPass final : public core::RenderPass {
public:
	virtual ~RenderPass() = default;

	bool init(Device &, const core::QueuePassData &);
};

// One recorded draw, fully resolved at record time: the executor only applies state and issues
// the GL call. The pipeline reference keeps the linked program alive for the whole execution.
struct SP_PUBLIC GlesDraw {
	Rc<core::GraphicPipeline> pipeline;

	GLuint texture = 0; // GL_TEXTURE_2D name for unit 0 (0 leaves the unit unbound)
	GLuint sampler = 0;
	int swizzle[4] = {0, 0, 0, 0}; // ComponentMapping per output channel, Identity(0) = same position

	uint32_t indexCount = 0;
	// uint32 entries into the recorded index buffer: the span's indexes rewritten there at record
	// time as absolute vertex ids (GLES has no base-vertex draw, so the rewrite is what makes a
	// plain glDrawElements work - exactly what the software rasterizer does with its list)
	uint32_t firstIndex = 0;
	uint32_t instanceCount = 1;
	uint32_t firstInstance = 0; // the value of uFirstInstance for this span

	URect scissor; // target pixels, already rotated and clamped by the recorder
};

// One vertex attribute of the subpass's layout, described by the recorder so the executor never
// has to know what a frame's vertices look like.
struct SP_PUBLIC GlesAttribute {
	GLuint location = 0;
	GLsizei size = 1;
	GLenum type = GL_FLOAT;
	bool normalized = false;
	uint32_t offset = 0;
};

// "Recording" here means building the draw list plus naming the buffers it draws from. The pass
// executes the whole thing at submit time, in order - which is exactly the painter's order the
// flat queue promises (the software backend records and rasterizes the same way).
class SP_PUBLIC CommandBuffer final : public core::CommandBuffer {
public:
	virtual ~CommandBuffer() = default;

	bool init(Device &);

	void addDraw(GlesDraw &&draw) { _draws.emplace_back(sp::move(draw)); }

	SpanView<const GlesDraw> getDraws() const { return _draws; }

	// The GL buffers this subpass draws from. The recorder uploads them and keeps its own
	// references alive through the execution (hold()); the executor only binds their names.
	GLuint vertexBuffer = 0;
	GLuint indexBuffer = 0;
	GLuint transformBuffer = 0; // SSBO, binding 0: the flat vertex shader's transforms

	uint32_t vertexStride = 0;
	Vector<GlesAttribute> vertexAttributes;

	// The execution runs after recordSubpass has returned, so a reference dropped there would free
	// its GL name before the draw list is issued. Keep every object a draw reads from alive until
	// then: the executor finishes with them, and only afterwards do they die (the clear callback
	// queues their delete for the next drain).
	void hold(Rc<core::BufferObject> &&object) { _held.emplace_back(sp::move(object)); }

protected:
	Device *_device = nullptr;
	Vector<GlesDraw> _draws;
	Vector<Rc<Ref>> _held;
};

// A queue attachment that carries an image. The backend adds nothing to it - like the soft one,
// it exists so a pass names its output with the backend's own type instead of core's.
class SP_PUBLIC ImageAttachment : public core::AttachmentTyped<core::AttachmentHandle, core::ImageAttachment> {
public:
	virtual ~ImageAttachment() = default;
};

class SP_PUBLIC QueuePassHandle : public core::QueuePassHandle {
public:
	virtual ~QueuePassHandle() = default;

	// Scene-space scissor -> target pixels, honouring the surface pre-rotation, and the source of
	// every glScissor this backend issues. Ported from soft: the transform is a property of the
	// presented surface, not of the API, so both backends have to agree on it or clipped content
	// would land in different places.
	static URect rotateScissor(const core::FrameConstraints &constraints, const URect &scissor);

	virtual bool prepare(core::FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(core::FrameQueue &, Rc<core::FrameSync> &&,
			Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) override;

protected:
	// Per-subpass recording hook: the default runs SubpassData::commandsCallback. The 2d flat
	// pass overrides it to upload its frame arrays and append one GlesDraw per span.
	virtual void recordSubpass(core::FrameQueue &, const core::SubpassData &, CommandBuffer &);

	// Apply the pass's load ops, then record and execute every subpass in order against the bound
	// framebuffer. Returns false when the pass has no usable target or a draw fails.
	bool runPass(core::FrameQueue &);

	// Walk one recorded buffer: build its VAO from the described attributes, bind its buffers and
	// issue each draw with state applied only on change. Runs where the context is current.
	bool executeDrawList(const CommandBuffer &);

	// Ask the swapchain's damage tracker what this frame actually has to redraw into the image it
	// was given, and record the answer in the three members below. Vulkan expresses it as a render
	// area and soft as a list of rasterized regions; GL has neither, so it is a scissor rectangle -
	// which bounds the load-op clears as well as the draws, because glClear* obeys the scissor
	// test. That makes the fragment work and the clear proportional to what changed; the vertex
	// work is not, and the redraw the frame skips entirely is where the real saving is.
	void preparePartialRedraw(core::FrameQueue &);

	Device *_device = nullptr;

	// The image already holds exactly this frame and the queue asked for SkipEmptyFrames: the pass
	// does nothing at all - no clear, no draws - and the image is presented as it stands.
	bool _skipRedraw = false;

	// Only _partialRedrawArea changed since this image last held a frame; everything outside it is
	// still valid and must be preserved.
	bool _partialRedraw = false;
	URect _partialRedrawArea;
};

} // namespace stappler::xenolith::gles

#endif /* XENOLITH_BACKEND_GLES_XLGLESQUEUEPASS_H_ */
