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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFTOBJECT_H_
#define XENOLITH_BACKEND_SOFT_XLSOFTOBJECT_H_

#include "XLSoftDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

// Plain host memory. "Device address" is the pointer itself, which is what makes the flat
// shader's BDA arithmetic collapse into ordinary pointer arithmetic.
class SP_PUBLIC Buffer final : public core::BufferObject {
public:
	virtual ~Buffer() = default;

	bool init(Device &, const core::BufferInfo &, BytesView initialData = BytesView());

	// create from resource data (data / memCallback / stdCallback sources)
	bool init(Device &, const core::BufferData &);

	uint8_t *getData() const { return const_cast<uint8_t *>(_storage.data()); }

	BytesView getView() const { return BytesView(_storage.data(), _storage.size()); }

protected:
	bool setup(Device &, const core::BufferInfo &,
			const Callback<size_t(uint8_t *, uint64_t)> *fill);

	Bytes _storage;
};

// Samplers carry no state beyond their description: the kernels read SamplerInfo directly.
class SP_PUBLIC Sampler final : public core::Sampler {
public:
	virtual ~Sampler() = default;

	bool init(Device &, const core::SamplerInfo &);
};

// A linear bitmap. Rows are tightly packed (stride == width * pixelSize): there is no hardware
// alignment to respect, and a predictable stride keeps the capture path a straight memcpy. An
// externally backed image keeps whatever stride its provider chose, but the capture path only
// accepts the packed one - see _external below.
class SP_PUBLIC Image final : public core::ImageObject {
public:
	virtual ~Image() = default;

	bool init(Device &, StringView, const core::ImageInfoData &);

	// with an explicit object index: swapchain images are keyed by their slot, which is what the
	// damage tracker indexes its per-image snapshots by
	bool init(Device &, StringView, const core::ImageInfoData &, uint64_t index);

	// Backed by memory the window system owns - a wl_shm buffer, an X SHM segment - so the
	// rasterizer writes the frame straight into what gets presented, with no copy in between.
	// The image does not own the memory and must outlive nothing: the swapchain that handed it
	// over keeps the mapping alive.
	bool init(Device &, StringView, const core::ImageInfoData &, uint64_t index, uint8_t *external,
			uint32_t stride, size_t size);

	// create from resource data (an already decoded bitmap)
	bool init(Device &, const core::ImageData &);

	uint8_t *getData() const {
		return _external ? _external : const_cast<uint8_t *>(_storage.data());
	}
	uint32_t getStride() const { return _stride; }

	BytesView getView() const {
		return _external ? BytesView(_external, _externalSize)
						 : BytesView(_storage.data(), _storage.size());
	}

	// Address of the first pixel of a layer, or null if the layer is out of range.
	uint8_t *getLayerData(uint32_t layer) const;

protected:
	bool setup(Device &, const core::ImageInfoData &,
			const Callback<size_t(uint8_t *, uint64_t)> *fill);

	Bytes _storage;
	// Window-system memory, when the image is externally backed. Non-owning: `_storage` stays
	// empty, the buffer is never cleared on creation (its previous frame is what partial redraw
	// diffs against), and `_externalSize` must cover exactly one slot - the capture path feeds it
	// to the bitmap encoder, which rejects any length that is not extent * bytes-per-pixel.
	uint8_t *_external = nullptr;
	size_t _externalSize = 0;
	uint32_t _stride = 0;
	uint32_t _layerSize = 0;
	// set only by the indexed init; the base class's _index already defaults to a valid value, so
	// it cannot double as "not requested"
	uint64_t _requestedIndex = maxOf<uint64_t>();
};

// There is no view object to create: this records the swizzle/range the sampler must apply and
// carries the index the framebuffer cache keys on.
class SP_PUBLIC ImageView final : public core::ImageView {
public:
	virtual ~ImageView() = default;

	bool init(Device &, const Rc<core::ImageObject> &, const core::ImageViewInfo &);
};

// Passive container for image views, like the WebGPU one - there is no framebuffer object.
class SP_PUBLIC Framebuffer final : public core::Framebuffer {
public:
	virtual ~Framebuffer() = default;

	bool init(Device &, const core::QueuePassData *, SpanView<Rc<core::ImageView>>);

	SpanView<Rc<core::ImageView>> getViews() const { return _imageViews; }
};

// Submission order is the task queue's order, so a semaphore has nothing to guard: it exists
// only to satisfy the frame graph's bookkeeping.
class SP_PUBLIC Semaphore final : public core::Semaphore {
public:
	virtual ~Semaphore() = default;

	bool init(Device &);
};

// Rasterization runs to completion inside submit, so a fence is only ever observed already
// signalled. It still has to be host-signallable: the pseudo-swapchain acquires images without
// a queue operation to attach the fence to.
class SP_PUBLIC Fence final : public core::Fence {
public:
	virtual ~Fence() = default;

	bool init(Device &, core::FenceType);

	void signal() { _signaled = true; }

protected:
	virtual Status doCheckFence(bool lockfree) override;
	virtual void doResetFence() override;

	sprt::atomic<bool> _signaled = true;
};

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFTOBJECT_H_ */
