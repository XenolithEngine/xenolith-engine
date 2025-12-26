/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOW_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOW_H_

#include "XL2dVkMaterial.h"
#include "XLCoreAttachment.h"
#include "XL2dCommandList.h"

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkQueuePass.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

class SP_PUBLIC ShadowLightDataAttachment : public core::GenericAttachment {
public:
	virtual ~ShadowLightDataAttachment() = default;

	virtual bool init(AttachmentBuilder &) override;

protected:
	using GenericAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class SP_PUBLIC ShadowLightDataAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~ShadowLightDataAttachmentHandle() = default;

	virtual void submitInput(FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&) override;

	void allocateBuffer(DeviceFrameHandle *, uint32_t gridCells, float maxValue = nan());

	float getBoxOffset(float value) const;

	uint32_t getLightsCount() const;

	const ShadowData &getShadowData() const { return _shadowData; }
	const FrameContextHandle2d *getFrameInput() const { return _input; }
	const Rc<Buffer> &getBuffer() const { return _data; }

protected:
	Rc<Buffer> _data;
	Rc<FrameContextHandle2d> _input;
	ShadowData _shadowData;
};

} // namespace stappler::xenolith::basic2d::vk
#endif

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOW_H_ */
