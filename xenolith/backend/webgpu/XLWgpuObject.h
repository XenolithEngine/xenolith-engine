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

#ifndef XENOLITH_BACKEND_WEBGPU_XLWGPUOBJECT_H_
#define XENOLITH_BACKEND_WEBGPU_XLWGPUOBJECT_H_

#include "XLWgpuDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {


SP_PUBLIC WGPUTextureUsage getWGPUTextureUsage(core::ImageUsage);
SP_PUBLIC WGPUBufferUsage getWGPUBufferUsage(core::BufferUsage);

class SP_PUBLIC Buffer final : public core::BufferObject {
public:
	virtual ~Buffer() = default;

	bool init(Device &, const core::BufferInfo &, BytesView initialData = BytesView());

	// create from resource data (data / memCallback / stdCallback sources)
	bool init(Device &, const core::BufferData &);

	WGPUBuffer getBuffer() const { return _buffer; }

protected:
	bool setup(Device &, const core::BufferInfo &,
			const Callback<size_t(uint8_t *, uint64_t)> *fill);

	WGPUBuffer _buffer = nullptr;
};

class SP_PUBLIC Sampler final : public core::Sampler {
public:
	virtual ~Sampler() = default;

	bool init(Device &, const core::SamplerInfo &);

	WGPUSampler getSampler() const { return _sampler; }

protected:
	WGPUSampler _sampler = nullptr;
};

class SP_PUBLIC Image final : public core::ImageObject {
public:
	virtual ~Image() = default;

	bool init(Device &, StringView, const core::ImageInfoData &);

	// wrap an externally-created texture (e.g. from wgpuSurfaceGetCurrentTexture);
	// takes ownership of the reference
	bool init(Device &, WGPUTexture, StringView, const core::ImageInfoData &);

	// create from resource data with direct upload (wgpuQueueWriteTexture)
	bool init(Device &, const core::ImageData &);

	WGPUTexture getTexture() const { return _texture; }

	// early release of the wgpu handle (before the wrapper object dies):
	// REQUIRED for presented surface textures - wgpu-native's texture wrapper
	// discards the surface's CURRENT texture when dropped after the next
	// acquire (has_surface_presented is a per-surface flag, reset by acquire)
	void invalidateTexture() {
		invalidate();
		_texture = nullptr;
	}

protected:
	WGPUTexture _texture = nullptr;
};

class SP_PUBLIC ImageView final : public core::ImageView {
public:
	virtual ~ImageView() = default;

	bool init(Device &, const Rc<core::ImageObject> &, const core::ImageViewInfo &);

	WGPUTextureView getTextureView() const { return _view; }

protected:
	WGPUTextureView _view = nullptr;
};

// WebGPU has no framebuffer object, this is a passive container for image views
class SP_PUBLIC Framebuffer final : public core::Framebuffer {
public:
	virtual ~Framebuffer() = default;

	bool init(Device &, const core::QueuePassData *, SpanView<Rc<core::ImageView>>);

	SpanView<Rc<core::ImageView>> getViews() const { return _imageViews; }
};

// WebGPU tracks submission order internally, semaphore is a state-only stub
// for the frame graph bookkeeping
class SP_PUBLIC Semaphore final : public core::Semaphore {
public:
	virtual ~Semaphore() = default;

	bool init(Device &);
};

// fence over wgpuQueueOnSubmittedWorkDone: callback raises the shared flag,
// doCheckFence observes it (callbacks are delivered on wgpuDevicePoll)
class SP_PUBLIC Fence final : public core::Fence {
public:
	struct SignalFlag : Ref {
		sprt::atomic<bool> signaled = false;
	};

	virtual ~Fence() = default;

	bool init(Device &, core::FenceType);

	// bind fence to the work, submitted to the queue; call after wgpuQueueSubmit
	void arm(WGPUQueue);

	// arm and signal immediately (for synchronous operations like surface acquire)
	void signal();

protected:
	virtual Status doCheckFence(bool lockfree) override;
	virtual void doResetFence() override;

	Rc<SignalFlag> _flag;
};

} // namespace stappler::xenolith::webgpu

#endif /* XENOLITH_BACKEND_WEBGPU_XLWGPUOBJECT_H_ */
