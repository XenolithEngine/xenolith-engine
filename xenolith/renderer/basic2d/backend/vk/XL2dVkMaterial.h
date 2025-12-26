/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKMATERIAL_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKMATERIAL_H_

#include "XL2d.h"
#include "XLCoreInfo.h"

#if MODULE_XENOLITH_BACKEND_VK

#include "XLVkAttachment.h"
#include "XLVkQueuePass.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

using namespace xenolith::vk;

class SP_PUBLIC MaterialAttachment : public core::MaterialAttachment {
public:
	virtual ~MaterialAttachment() = default;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;

	virtual void setCompiled(core::Device &) override;

	virtual Bytes getMaterialData(NotNull<core::Material>) const override;

	virtual Rc<core::BufferObject> allocateMaterialPersistentBuffer(
			NotNull<core::Material>) const override;

protected:
	size_t getMaterialSize(NotNull<core::Material>) const;

	Rc<DeviceMemoryPool> _pool;
};

class MaterialAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~MaterialAttachmentHandle() = default;

	virtual bool init(const Rc<Attachment> &, const FrameQueue &) override;

	const MaterialAttachment *getMaterialAttachment() const;

	const Rc<core::MaterialSet> getSet() const;

protected:
	std::mutex _mutex;
	mutable Rc<core::MaterialSet> _materials;
};

} // namespace stappler::xenolith::basic2d::vk

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKMATERIAL_H_ */
