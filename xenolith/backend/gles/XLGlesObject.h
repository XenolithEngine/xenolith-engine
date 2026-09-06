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

#ifndef XENOLITH_BACKEND_GLES_XLGLESOBJECT_H_
#define XENOLITH_BACKEND_GLES_XLGLESOBJECT_H_

#include "XLGlesDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// Every GL object name lives in the core ObjectHandle word (see glObjectHandle). The clear
// callbacks run on whatever thread drops the last reference, so they never touch the API: they
// hand a single delete call to the device's deferred queue, which drains it on the loop thread
// where the context is current.
class SP_PUBLIC Buffer final : public core::BufferObject {
public:
	virtual ~Buffer() = default;

	bool init(Device &, const core::BufferData &);

	// The generic buffer target: one name usable for both upload and glMapBufferRange readback,
	// which is what captureBuffer needs.
	GLuint getGlName() const { return _glBuffer; }

protected:
	GLuint _glBuffer = 0;
};

// A 2D texture created with immutable storage (glTexStorage2D): the allocation happens once at
// init, and everything after it is a plain glTexSubImage2D. Rows are tightly packed on upload
// (GL_UNPACK_ALIGNMENT=1), matching the ImageInfoData contract. Mipmaps do not exist here:
// single-level textures only, which also bounds what the samplers can say.
class SP_PUBLIC Image final : public core::ImageObject {
public:
	virtual ~Image() = default;

	bool init(Device &, StringView, const core::ImageInfoData &);

	// with an explicit object index: swapchain images are keyed by their slot (0 included; pass
	// maxOf<uint64_t>() to let the device assign one)
	bool init(Device &, StringView, const core::ImageInfoData &, uint64_t index);

	// with initial pixel bytes: the upload happens inside setup, so a dynamic image can be
	// compiled straight from what acquireData hands over
	bool init(Device &, StringView, const core::ImageInfoData &, BytesView initialData);

	// create from resource data (an already decoded bitmap) and upload it in one go
	bool init(Device &, const core::ImageData &);

	GLuint getGlName() const { return _glTexture; }

protected:
	bool setup(Device &, const core::ImageInfoData &,
			const Callback<size_t(uint8_t *, uint64_t)> *fill,
			uint64_t requestedIndex = maxOf<uint64_t>());

	GLuint _glTexture = 0;
};

// There is no view object in GLES: this records the swizzle/range and carries the index the
// framebuffer cache keys on. The texture it names comes from the image at bind time.
class SP_PUBLIC ImageView final : public core::ImageView {
public:
	virtual ~ImageView() = default;

	bool init(Device &, const Rc<core::ImageObject> &, const core::ImageViewInfo &);
};

// A real sampler object (GLES 3.0+): the filter/wrap state travels with it, so a draw does not
// have to touch texture parameters at all. Identical requests share one instance through
// Device::getSampler, which is what keeps a large scene from exhausting the binding slots.
class SP_PUBLIC Sampler final : public core::Sampler {
public:
	virtual ~Sampler() = default;

	bool init(Device &, const core::SamplerInfo &);

	GLuint getGlName() const { return _glSampler; }

protected:
	GLuint _glSampler = 0;
};

// A real framebuffer object. The attachment list is fixed at creation (the views the pass
// declares), and completeness is checked once then: nothing about it can change afterwards, so
// there is no state to transition between frames.
class SP_PUBLIC Framebuffer final : public core::Framebuffer {
public:
	virtual ~Framebuffer() = default;

	bool init(Device &, const core::QueuePassData *, SpanView<Rc<core::ImageView>>);

	GLuint getGlName() const { return _glFbo; }
	bool isComplete() const { return _complete; }

protected:
	GLuint _glFbo = 0;
	bool _complete = false;
};

// Submission order is the loop thread's order, so a semaphore has nothing to guard: it exists
// only to satisfy the frame graph's bookkeeping.
class SP_PUBLIC Semaphore final : public core::Semaphore {
public:
	virtual ~Semaphore() = default;

	bool init(Device &);
};

// Two ways of being signalled:
// - on submit, a GLsync is created after the pass work and checked with glClientWaitSync — this
//   is what orders a frame's completion against its resources;
// - host-side, when nothing was submitted for it (the pseudo-swapchain acquires an image without
//   a queue operation): signal() flips the flag and doCheckFence reports it immediately.
class SP_PUBLIC Fence final : public core::Fence {
public:
	virtual ~Fence() = default;

	bool init(Device &, core::FenceType);

	// host-side signalling, for the acquire path that has no GL work behind it
	void signal() { _signaled.store(true); }

	GLsync getGlSync() const { return _glSync; }

	// Attaching a live sync object arms the fence: it must read as unsignalled until
	// glClientWaitSync reports completion (or doResetFence detaches it). A null sync is just
	// detachment and leaves the host flag alone.
	void setGlSync(GLsync sync) {
		_glSync = sync;
		if (sync != nullptr) { _signaled.store(false); }
	}

protected:
	virtual Status doCheckFence(bool lockfree) override;
	virtual void doResetFence() override;

	sprt::atomic<bool> _signaled = true; // a fence that was never armed reads as signalled
	GLsync _glSync = nullptr;
};

} // namespace stappler::xenolith::gles

#endif /* XENOLITH_BACKEND_GLES_XLGLESOBJECT_H_ */
