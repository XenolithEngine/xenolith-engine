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

#ifndef XENOLITH_BACKEND_MTL_XLMTLOBJECT_H_
#define XENOLITH_BACKEND_MTL_XLMTLOBJECT_H_

#include "XLMtlDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

class SP_PUBLIC Buffer final : public core::BufferObject {
public:
	virtual ~Buffer() = default;

	bool init(Device &, const core::BufferInfo &, BytesView initialData = BytesView());

	// create from resource data (data / memCallback / stdCallback sources)
	bool init(Device &, const core::BufferData &);

	// direct CPU write into the shared-storage buffer (coherent on unified
	// memory); the caller is responsible for GPU/CPU ordering
	bool setData(BytesView, uint64_t offset = 0);

#if __OBJC__
	id<MTLBuffer> getBuffer() const { return bridgeHandle<id<MTLBuffer>>(_buffer); }
#endif

protected:
	bool setup(Device &, const core::BufferInfo &,
			const Callback<size_t(uint8_t *, uint64_t)> *fill);

	void *_buffer = nullptr; // __bridge_retained id<MTLBuffer>
};

class SP_PUBLIC Sampler final : public core::Sampler {
public:
	virtual ~Sampler() = default;

	bool init(Device &, const core::SamplerInfo &);

#if __OBJC__
	id<MTLSamplerState> getSampler() const {
		return bridgeHandle<id<MTLSamplerState>>(_sampler);
	}
#endif

protected:
	void *_sampler = nullptr; // __bridge_retained id<MTLSamplerState>
};

class SP_PUBLIC Image final : public core::ImageObject {
public:
	virtual ~Image() = default;

	bool init(Device &, StringView, const core::ImageInfoData &);

	// create from resource data with direct upload (replaceRegion on a
	// shared-storage texture)
	bool init(Device &, const core::ImageData &);

	// direct CPU upload into the shared-storage texture (replaceRegion);
	// data is tightly packed rows of the full extent, level 0
	bool setData(BytesView);

#if __OBJC__
	// wrap an externally-created texture (e.g. a CAMetalDrawable's texture);
	// retains the reference
	bool init(Device &, id<MTLTexture>, StringView, const core::ImageInfoData &);

	id<MTLTexture> getTexture() const { return bridgeHandle<id<MTLTexture>>(_texture); }
#endif

	// early release of the texture handle (before the wrapper object dies):
	// drawable textures must not outlive their CAMetalDrawable presentation
	void invalidateTexture() {
		invalidate();
		_texture = nullptr;
	}

protected:
	void *_texture = nullptr; // __bridge_retained id<MTLTexture>
};

class SP_PUBLIC ImageView final : public core::ImageView {
public:
	virtual ~ImageView() = default;

	bool init(Device &, const Rc<core::ImageObject> &, const core::ImageViewInfo &);

#if __OBJC__
	// texture view created with newTextureViewWithPixelFormat (or the parent
	// texture itself when the view is an identity view)
	id<MTLTexture> getTextureView() const { return bridgeHandle<id<MTLTexture>>(_view); }
#endif

protected:
	void *_view = nullptr; // __bridge_retained id<MTLTexture>
};

// Metal has no framebuffer object, this is a passive container for image views
class SP_PUBLIC Framebuffer final : public core::Framebuffer {
public:
	virtual ~Framebuffer() = default;

	bool init(Device &, const core::QueuePassData *, SpanView<Rc<core::ImageView>>);

	SpanView<Rc<core::ImageView>> getViews() const { return _imageViews; }
};

// Metal tracks command buffer order within a queue internally, semaphore is a
// state-only stub for the frame graph bookkeeping
class SP_PUBLIC Semaphore final : public core::Semaphore {
public:
	virtual ~Semaphore() = default;

	bool init(Device &);
};

// fence over MTLCommandBuffer addCompletedHandler: the handler raises the
// shared flag, doCheckFence observes it
class SP_PUBLIC Fence final : public core::Fence {
public:
	struct SignalFlag : Ref {
		sprt::atomic<bool> signaled = false;
	};

	virtual ~Fence() = default;

	bool init(Device &, core::FenceType);

#if __OBJC__
	// bind fence to the command buffer; call before commit
	void arm(id<MTLCommandBuffer>);
#endif

	// arm and signal immediately (for synchronous operations like drawable acquire)
	void signal();

protected:
	virtual Status doCheckFence(bool lockfree) override;
	virtual void doResetFence() override;

	Rc<SignalFlag> _flag;
};

} // namespace stappler::xenolith::mtl

#endif /* XENOLITH_BACKEND_MTL_XLMTLOBJECT_H_ */
